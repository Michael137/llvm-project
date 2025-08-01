//===- BufferDeallocationSimplification.cpp -------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements logic for optimizing `bufferization.dealloc` operations
// that requires more analysis than what can be supported by regular
// canonicalization patterns.
//
//===----------------------------------------------------------------------===//

#include "mlir/Dialect/Bufferization/IR/Bufferization.h"
#include "mlir/Dialect/Bufferization/Transforms/BufferViewFlowAnalysis.h"
#include "mlir/Dialect/Bufferization/Transforms/Passes.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Matchers.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"

namespace mlir {
namespace bufferization {
#define GEN_PASS_DEF_BUFFERDEALLOCATIONSIMPLIFICATIONPASS
#include "mlir/Dialect/Bufferization/Transforms/Passes.h.inc"
} // namespace bufferization
} // namespace mlir

using namespace mlir;
using namespace mlir::bufferization;

//===----------------------------------------------------------------------===//
// Helpers
//===----------------------------------------------------------------------===//

/// Given a memref value, return the "base" value by skipping over all
/// ViewLikeOpInterface ops (if any) in the reverse use-def chain.
static Value getViewBase(Value value) {
  while (auto viewLikeOp = value.getDefiningOp<ViewLikeOpInterface>())
    value = viewLikeOp.getViewSource();
  return value;
}

static LogicalResult updateDeallocIfChanged(DeallocOp deallocOp,
                                            ValueRange memrefs,
                                            ValueRange conditions,
                                            PatternRewriter &rewriter) {
  if (deallocOp.getMemrefs() == memrefs &&
      deallocOp.getConditions() == conditions)
    return failure();

  rewriter.modifyOpInPlace(deallocOp, [&]() {
    deallocOp.getMemrefsMutable().assign(memrefs);
    deallocOp.getConditionsMutable().assign(conditions);
  });
  return success();
}

/// Return "true" if the given values are guaranteed to be different (and
/// non-aliasing) allocations based on the fact that one value is the result
/// of an allocation and the other value is a block argument of a parent block.
/// Note: This is a best-effort analysis that will eventually be replaced by a
/// proper "is same allocation" analysis. This function may return "false" even
/// though the two values are distinct allocations.
static bool distinctAllocAndBlockArgument(Value v1, Value v2) {
  Value v1Base = getViewBase(v1);
  Value v2Base = getViewBase(v2);
  auto areDistinct = [](Value v1, Value v2) {
    if (Operation *op = v1.getDefiningOp())
      if (hasEffect<MemoryEffects::Allocate>(op, v1))
        if (auto bbArg = dyn_cast<BlockArgument>(v2))
          if (bbArg.getOwner()->findAncestorOpInBlock(*op))
            return true;
    return false;
  };
  return areDistinct(v1Base, v2Base) || areDistinct(v2Base, v1Base);
}

/// Checks if `memref` may potentially alias a MemRef in `otherList`. It is
/// often a requirement of optimization patterns that there cannot be any
/// aliasing memref in order to perform the desired simplification.
static bool potentiallyAliasesMemref(BufferOriginAnalysis &analysis,
                                     ValueRange otherList, Value memref) {
  for (auto other : otherList) {
    if (distinctAllocAndBlockArgument(other, memref))
      continue;
    std::optional<bool> analysisResult =
        analysis.isSameAllocation(other, memref);
    if (!analysisResult.has_value() || analysisResult == true)
      return true;
  }
  return false;
}

//===----------------------------------------------------------------------===//
// Patterns
//===----------------------------------------------------------------------===//

namespace {

/// Remove values from the `memref` operand list that are also present in the
/// `retained` list (or a guaranteed alias of it) because they will never
/// actually be deallocated. However, we also need to be certain about which
/// other memrefs in the `retained` list can alias, i.e., there must not by any
/// may-aliasing memref. This is necessary because the `dealloc` operation is
/// defined to return one `i1` value per memref in the `retained` list which
/// represents the disjunction of the condition values corresponding to all
/// aliasing values in the `memref` list. In particular, this means that if
/// there is some value R in the `retained` list which aliases with a value M in
/// the `memref` list (but can only be staticaly determined to may-alias) and M
/// is also present in the `retained` list, then it would be illegal to remove M
/// because the result corresponding to R would be computed incorrectly
/// afterwards.  Because we require an alias analysis, this pattern cannot be
/// applied as a regular canonicalization pattern.
///
/// Example:
/// ```mlir
/// %0:3 = bufferization.dealloc (%m0 : ...) if (%cond0)
///                     retain (%m0, %r0, %r1 : ...)
/// ```
/// is canonicalized to
/// ```mlir
/// // bufferization.dealloc without memrefs and conditions returns %false for
/// // every retained value
/// %0:3 = bufferization.dealloc retain (%m0, %r0, %r1 : ...)
/// %1 = arith.ori %0#0, %cond0 : i1
/// // replace %0#0 with %1
/// ```
/// given that `%r0` and `%r1` may not alias with `%m0`.
struct RemoveDeallocMemrefsContainedInRetained
    : public OpRewritePattern<DeallocOp> {
  RemoveDeallocMemrefsContainedInRetained(MLIRContext *context,
                                          BufferOriginAnalysis &analysis)
      : OpRewritePattern<DeallocOp>(context), analysis(analysis) {}

  /// The passed 'memref' must not have a may-alias relation to any retained
  /// memref, and at least one must-alias relation. If there is no must-aliasing
  /// memref in the retain list, we cannot simply remove the memref as there
  /// could be situations in which it actually has to be deallocated. If it's
  /// no-alias, then just proceed, if it's must-alias we need to update the
  /// updated condition returned by the dealloc operation for that alias.
  LogicalResult handleOneMemref(DeallocOp deallocOp, Value memref, Value cond,
                                PatternRewriter &rewriter) const {
    rewriter.setInsertionPointAfter(deallocOp);

    // Check that there is no may-aliasing memref and that at least one memref
    // in the retain list aliases (because otherwise it might have to be
    // deallocated in some situations and can thus not be dropped).
    bool atLeastOneMustAlias = false;
    for (Value retained : deallocOp.getRetained()) {
      std::optional<bool> analysisResult =
          analysis.isSameAllocation(retained, memref);
      if (!analysisResult.has_value())
        return failure();
      if (analysisResult == true)
        atLeastOneMustAlias = true;
    }
    if (!atLeastOneMustAlias)
      return failure();

    // Insert arith.ori operations to update the corresponding dealloc result
    // values to incorporate the condition of the must-aliasing memref such that
    // we can remove that operand later on.
    for (auto [i, retained] : llvm::enumerate(deallocOp.getRetained())) {
      Value updatedCondition = deallocOp.getUpdatedConditions()[i];
      std::optional<bool> analysisResult =
          analysis.isSameAllocation(retained, memref);
      if (analysisResult == true) {
        auto disjunction = arith::OrIOp::create(rewriter, deallocOp.getLoc(),
                                                updatedCondition, cond);
        rewriter.replaceAllUsesExcept(updatedCondition, disjunction.getResult(),
                                      disjunction);
      }
    }

    return success();
  }

  LogicalResult matchAndRewrite(DeallocOp deallocOp,
                                PatternRewriter &rewriter) const override {
    // There must not be any duplicates in the retain list anymore because we
    // would miss updating one of the result values otherwise.
    DenseSet<Value> retained(deallocOp.getRetained().begin(),
                             deallocOp.getRetained().end());
    if (retained.size() != deallocOp.getRetained().size())
      return failure();

    SmallVector<Value> newMemrefs, newConditions;
    for (auto [memref, cond] :
         llvm::zip(deallocOp.getMemrefs(), deallocOp.getConditions())) {

      if (succeeded(handleOneMemref(deallocOp, memref, cond, rewriter)))
        continue;

      if (auto extractOp =
              memref.getDefiningOp<memref::ExtractStridedMetadataOp>())
        if (succeeded(handleOneMemref(deallocOp, extractOp.getOperand(), cond,
                                      rewriter)))
          continue;

      newMemrefs.push_back(memref);
      newConditions.push_back(cond);
    }

    // Return failure if we don't change anything such that we don't run into an
    // infinite loop of pattern applications.
    return updateDeallocIfChanged(deallocOp, newMemrefs, newConditions,
                                  rewriter);
  }

private:
  BufferOriginAnalysis &analysis;
};

/// Remove memrefs from the `retained` list which are guaranteed to not alias
/// any memref in the `memrefs` list. The corresponding result value can be
/// replaced with `false` in that case according to the operation description.
///
/// Example:
/// ```mlir
/// %0:2 = bufferization.dealloc (%m : memref<2xi32>) if (%cond)
///                       retain (%r0, %r1 : memref<2xi32>, memref<2xi32>)
/// return %0#0, %0#1
/// ```
/// can be canonicalized to the following given that `%r0` and `%r1` do not
/// alias `%m`:
/// ```mlir
/// bufferization.dealloc (%m : memref<2xi32>) if (%cond)
/// return %false, %false
/// ```
struct RemoveRetainedMemrefsGuaranteedToNotAlias
    : public OpRewritePattern<DeallocOp> {
  RemoveRetainedMemrefsGuaranteedToNotAlias(MLIRContext *context,
                                            BufferOriginAnalysis &analysis)
      : OpRewritePattern<DeallocOp>(context), analysis(analysis) {}

  LogicalResult matchAndRewrite(DeallocOp deallocOp,
                                PatternRewriter &rewriter) const override {
    SmallVector<Value> newRetainedMemrefs, replacements;

    for (auto retainedMemref : deallocOp.getRetained()) {
      if (potentiallyAliasesMemref(analysis, deallocOp.getMemrefs(),
                                   retainedMemref)) {
        newRetainedMemrefs.push_back(retainedMemref);
        replacements.push_back({});
        continue;
      }

      replacements.push_back(arith::ConstantOp::create(
          rewriter, deallocOp.getLoc(), rewriter.getBoolAttr(false)));
    }

    if (newRetainedMemrefs.size() == deallocOp.getRetained().size())
      return failure();

    auto newDeallocOp =
        DeallocOp::create(rewriter, deallocOp.getLoc(), deallocOp.getMemrefs(),
                          deallocOp.getConditions(), newRetainedMemrefs);
    int i = 0;
    for (auto &repl : replacements) {
      if (!repl)
        repl = newDeallocOp.getUpdatedConditions()[i++];
    }

    rewriter.replaceOp(deallocOp, replacements);
    return success();
  }

private:
  BufferOriginAnalysis &analysis;
};

/// Split off memrefs to separate dealloc operations to reduce the number of
/// runtime checks required and enable further canonicalization of the new and
/// simpler dealloc operations. A memref can be split off if it is guaranteed to
/// not alias with any other memref in the `memref` operand list.  The results
/// of the old and the new dealloc operation have to be combined by computing
/// the element-wise disjunction of them.
///
/// Example:
/// ```mlir
/// %0:2 = bufferization.dealloc (%m0, %m1 : memref<2xi32>, memref<2xi32>)
///                           if (%cond0, %cond1)
///                       retain (%r0, %r1 : memref<2xi32>, memref<2xi32>)
/// return %0#0, %0#1
/// ```
/// Given that `%m0` is guaranteed to never alias with `%m1`, the above IR is
/// canonicalized to the following, thus reducing the number of runtime alias
/// checks by 1 and potentially enabling further canonicalization of the new
/// split-up dealloc operations.
/// ```mlir
/// %0:2 = bufferization.dealloc (%m0 : memref<2xi32>) if (%cond0)
///                       retain (%r0, %r1 : memref<2xi32>, memref<2xi32>)
/// %1:2 = bufferization.dealloc (%m1 : memref<2xi32>) if (%cond1)
///                       retain (%r0, %r1 : memref<2xi32>, memref<2xi32>)
/// %2 = arith.ori %0#0, %1#0
/// %3 = arith.ori %0#1, %1#1
/// return %2, %3
/// ```
struct SplitDeallocWhenNotAliasingAnyOther
    : public OpRewritePattern<DeallocOp> {
  SplitDeallocWhenNotAliasingAnyOther(MLIRContext *context,
                                      BufferOriginAnalysis &analysis)
      : OpRewritePattern<DeallocOp>(context), analysis(analysis) {}

  LogicalResult matchAndRewrite(DeallocOp deallocOp,
                                PatternRewriter &rewriter) const override {
    Location loc = deallocOp.getLoc();
    if (deallocOp.getMemrefs().size() <= 1)
      return failure();

    SmallVector<Value> remainingMemrefs, remainingConditions;
    SmallVector<SmallVector<Value>> updatedConditions;
    for (int64_t i = 0, e = deallocOp.getMemrefs().size(); i < e; ++i) {
      Value memref = deallocOp.getMemrefs()[i];
      Value cond = deallocOp.getConditions()[i];
      SmallVector<Value> otherMemrefs(deallocOp.getMemrefs());
      otherMemrefs.erase(otherMemrefs.begin() + i);
      // Check if `memref` can split off into a separate bufferization.dealloc.
      if (potentiallyAliasesMemref(analysis, otherMemrefs, memref)) {
        // `memref` alias with other memrefs, do not split off.
        remainingMemrefs.push_back(memref);
        remainingConditions.push_back(cond);
        continue;
      }

      // Create new bufferization.dealloc op for `memref`.
      auto newDeallocOp = DeallocOp::create(rewriter, loc, memref, cond,
                                            deallocOp.getRetained());
      updatedConditions.push_back(
          llvm::to_vector(ValueRange(newDeallocOp.getUpdatedConditions())));
    }

    // Fail if no memref was split off.
    if (remainingMemrefs.size() == deallocOp.getMemrefs().size())
      return failure();

    // Create bufferization.dealloc op for all remaining memrefs.
    auto newDeallocOp =
        DeallocOp::create(rewriter, loc, remainingMemrefs, remainingConditions,
                          deallocOp.getRetained());

    // Bit-or all conditions.
    SmallVector<Value> replacements =
        llvm::to_vector(ValueRange(newDeallocOp.getUpdatedConditions()));
    for (auto additionalConditions : updatedConditions) {
      assert(replacements.size() == additionalConditions.size() &&
             "expected same number of updated conditions");
      for (int64_t i = 0, e = replacements.size(); i < e; ++i) {
        replacements[i] = arith::OrIOp::create(rewriter, loc, replacements[i],
                                               additionalConditions[i]);
      }
    }
    rewriter.replaceOp(deallocOp, replacements);
    return success();
  }

private:
  BufferOriginAnalysis &analysis;
};

/// Check for every retained memref if a must-aliasing memref exists in the
/// 'memref' operand list with constant 'true' condition. If so, we can replace
/// the operation result corresponding to that retained memref with 'true'. If
/// this condition holds for all retained memrefs we can also remove the
/// aliasing memrefs and their conditions since they will never be deallocated
/// due to the must-alias and we don't need them to compute the result value
/// anymore since it got replaced with 'true'.
///
/// Example:
/// ```mlir
/// %0:2 = bufferization.dealloc (%arg0, %arg1, %arg2 : ...)
///                           if (%true, %true, %true)
///                       retain (%arg0, %arg1 : memref<2xi32>, memref<2xi32>)
/// ```
/// becomes
/// ```mlir
/// %0:2 = bufferization.dealloc (%arg2 : memref<2xi32>) if (%true)
///                       retain (%arg0, %arg1 : memref<2xi32>, memref<2xi32>)
/// // replace %0#0 with %true
/// // replace %0#1 with %true
/// ```
/// Note that the dealloc operation will still have the result values, but they
/// don't have uses anymore.
struct RetainedMemrefAliasingAlwaysDeallocatedMemref
    : public OpRewritePattern<DeallocOp> {
  RetainedMemrefAliasingAlwaysDeallocatedMemref(MLIRContext *context,
                                                BufferOriginAnalysis &analysis)
      : OpRewritePattern<DeallocOp>(context), analysis(analysis) {}

  LogicalResult matchAndRewrite(DeallocOp deallocOp,
                                PatternRewriter &rewriter) const override {
    BitVector aliasesWithConstTrueMemref(deallocOp.getRetained().size());
    SmallVector<Value> newMemrefs, newConditions;
    for (auto [memref, cond] :
         llvm::zip(deallocOp.getMemrefs(), deallocOp.getConditions())) {
      bool canDropMemref = false;
      for (auto [i, retained, res] : llvm::enumerate(
               deallocOp.getRetained(), deallocOp.getUpdatedConditions())) {
        if (!matchPattern(cond, m_One()))
          continue;

        std::optional<bool> analysisResult =
            analysis.isSameAllocation(retained, memref);
        if (analysisResult == true) {
          rewriter.replaceAllUsesWith(res, cond);
          aliasesWithConstTrueMemref[i] = true;
          canDropMemref = true;
          continue;
        }

        // TODO: once our alias analysis is powerful enough we can remove the
        // rest of this loop body
        auto extractOp =
            memref.getDefiningOp<memref::ExtractStridedMetadataOp>();
        if (!extractOp)
          continue;

        std::optional<bool> extractAnalysisResult =
            analysis.isSameAllocation(retained, extractOp.getOperand());
        if (extractAnalysisResult == true) {
          rewriter.replaceAllUsesWith(res, cond);
          aliasesWithConstTrueMemref[i] = true;
          canDropMemref = true;
        }
      }

      if (!canDropMemref) {
        newMemrefs.push_back(memref);
        newConditions.push_back(cond);
      }
    }
    if (!aliasesWithConstTrueMemref.all())
      return failure();

    return updateDeallocIfChanged(deallocOp, newMemrefs, newConditions,
                                  rewriter);
  }

private:
  BufferOriginAnalysis &analysis;
};

} // namespace

//===----------------------------------------------------------------------===//
// BufferDeallocationSimplificationPass
//===----------------------------------------------------------------------===//

namespace {

/// The actual buffer deallocation pass that inserts and moves dealloc nodes
/// into the right positions. Furthermore, it inserts additional clones if
/// necessary. It uses the algorithm described at the top of the file.
struct BufferDeallocationSimplificationPass
    : public bufferization::impl::BufferDeallocationSimplificationPassBase<
          BufferDeallocationSimplificationPass> {
  void runOnOperation() override {
    BufferOriginAnalysis analysis(getOperation());
    RewritePatternSet patterns(&getContext());
    patterns.add<RemoveDeallocMemrefsContainedInRetained,
                 RemoveRetainedMemrefsGuaranteedToNotAlias,
                 SplitDeallocWhenNotAliasingAnyOther,
                 RetainedMemrefAliasingAlwaysDeallocatedMemref>(&getContext(),
                                                                analysis);

    populateDeallocOpCanonicalizationPatterns(patterns, &getContext());
    // We don't want that the block structure changes invalidating the
    // `BufferOriginAnalysis` so we apply the rewrites with `Normal` level of
    // region simplification
    if (failed(applyPatternsGreedily(
            getOperation(), std::move(patterns),
            GreedyRewriteConfig().setRegionSimplificationLevel(
                GreedySimplifyRegionLevel::Normal))))
      signalPassFailure();
  }
};

} // namespace
