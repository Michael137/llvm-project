//===------------------ ItaniumDemangleTestUtils.h ------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_DEMANGLE_ITANIUMDEMANGLE_TEST_UTILS_H
#define LLVM_DEMANGLE_ITANIUMDEMANGLE_TEST_UTILS_H

#include "llvm/Demangle/Demangle.h"
#include "llvm/Demangle/ItaniumDemangle.h"
#include "llvm/Demangle/StringView.h"
#include "llvm/Support/Allocator.h"

#include <vector>

namespace llvm::itanium_demangle::test_utils {

class TestAllocator {
  BumpPtrAllocator Alloc;

public:
  void reset() { Alloc.Reset(); }

  template <typename T, typename... Args> T *makeNode(Args &&...args) {
    return new (Alloc.Allocate(sizeof(T), alignof(T)))
        T(std::forward<Args>(args)...);
  }

  void *allocateNodeArray(size_t sz) {
    return Alloc.Allocate(sizeof(Node *) * sz, alignof(Node *));
  }
};

struct ManglingNodeCreator
    : AbstractManglingParser<ManglingNodeCreator, TestAllocator> {

  ManglingNodeCreator() : AbstractManglingParser(nullptr, nullptr) {}

  auto *makeTemplateArgs(std::vector<Node *> const &Args) {
    return make<TemplateArgs>(makeNodeArray(Args.cbegin(), Args.cend()));
  }

  auto *makeQualType(StringView VarName, Qualifiers Quals) {
    return make<QualType>(make<NameType>(VarName), Quals);
  }

  auto *makeReferenceType(Qualifiers Quals, StringView ArgName,
                          ReferenceKind RefKind) {
    return make<ReferenceType>(makeQualType(ArgName, Quals), RefKind);
  }

  auto *makeNameWithTemplateArgs(StringView FnName,
                                 std::vector<Node *> const &Args) {
    auto *TArgs = makeTemplateArgs(Args);
    return make<NameWithTemplateArgs>(make<NameType>(FnName), TArgs);
  }

  auto *makeFunctionEncoding(StringView FnName, std::vector<Node *> const &Args,
                             Qualifiers CVQuals, FunctionRefQual RefQual) {
    auto const *NameNode = make<NameType>(FnName);
    NodeArray Params = makeNodeArray(Args.cbegin(), Args.cend());
    return make<FunctionEncoding>(nullptr, NameNode, Params, nullptr, CVQuals,
                                  RefQual);
  }
};

inline bool compareNodes(Node const *LHS, Node const *RHS) {
  return LHS && RHS && LHS->equals(RHS);
}

} // namespace llvm::itanium_demangle::test_utils

#endif // LLVM_DEMANGLE_ITANIUMDEMANGLE_TEST_UTILS_H
