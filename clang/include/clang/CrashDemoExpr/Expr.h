#ifndef LLVM_CLANG_CRASH_DEMO_EXPR_H
#define LLVM_CLANG_CRASH_DEMO_EXPR_H

#include "clang/AST/APValue.h"
#include "clang/AST/ASTVector.h"
#include "clang/AST/ComputeDependence.h"
#include "clang/AST/Decl.h"
#include "clang/AST/ODRHash.h"
#include "clang/AST/StmtIterator.h"
#include "clang/AST/DeclAccessPair.h"
#include "clang/AST/DependenceFlags.h"
#include "clang/AST/OperationKinds.h"
#include "clang/AST/TemplateBase.h"
#include "clang/AST/Type.h"
#include "clang/Basic/CharInfo.h"
#include "clang/Basic/LangOptions.h"
#include "clang/Basic/SyncScope.h"
#include "clang/Basic/TypeTraits.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/APSInt.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/iterator.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/Support/AtomicOrdering.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/TrailingObjects.h"

namespace clang {
namespace crashing {
  class ASTContext;
  class BlockDecl;
  class CXXBaseSpecifier;
  class CXXMemberCallExpr;
  class CXXOperatorCallExpr;
  class CastExpr;
  class Decl;
  class IdentifierInfo;
  class MaterializeTemporaryExpr;
  class NamedDecl;
  class ObjCPropertyRefExpr;
  class OpaqueValueExpr;
  class ParmVarDecl;
  class StringLiteral;
  class TargetInfo;
  class ValueDecl;

/// A simple array of base specifiers.
typedef SmallVector<CXXBaseSpecifier*, 4> CXXCastPath;

/// An adjustment to be made to the temporary created when emitting a
/// reference binding, which accesses a particular subobject of that temporary.
struct SubobjectAdjustment {
  enum {
    DerivedToBaseAdjustment,
    FieldAdjustment,
    MemberPointerAdjustment
  } Kind;

  struct DTB {
    const CastExpr *BasePath;
    const CXXRecordDecl *DerivedClass;
  };

  struct P {
    const MemberPointerType *MPT;
    Expr *RHS;
  };

  union {
    struct DTB DerivedToBase;
    FieldDecl *Field;
    struct P Ptr;
  };

  SubobjectAdjustment(const CastExpr *BasePath,
                      const CXXRecordDecl *DerivedClass)
    : Kind(DerivedToBaseAdjustment) {
    DerivedToBase.BasePath = BasePath;
    DerivedToBase.DerivedClass = DerivedClass;
  }

  SubobjectAdjustment(FieldDecl *Field)
    : Kind(FieldAdjustment) {
    this->Field = Field;
  }

  SubobjectAdjustment(const MemberPointerType *MPT, Expr *RHS)
    : Kind(MemberPointerAdjustment) {
    this->Ptr.MPT = MPT;
    this->Ptr.RHS = RHS;
  }
};

class alignas(void *) Stmt {
public:
  enum StmtClass {
    NoStmtClass = 0,
#define STMT(CLASS, PARENT) CLASS##Class,
#define STMT_RANGE(BASE, FIRST, LAST) \
        first##BASE##Constant=FIRST##Class, last##BASE##Constant=LAST##Class,
#define LAST_STMT_RANGE(BASE, FIRST, LAST) \
        first##BASE##Constant=FIRST##Class, last##BASE##Constant=LAST##Class
#define ABSTRACT_STMT(STMT)
#include "clang/AST/StmtNodes.inc"
  };

  // Make vanilla 'new' and 'delete' illegal for Stmts.
protected:
  friend class ASTStmtReader;
  friend class ASTStmtWriter;

  void *operator new(size_t bytes) noexcept {
    llvm_unreachable("Stmts cannot be allocated with regular 'new'.");
  }

  void operator delete(void *data) noexcept {
    llvm_unreachable("Stmts cannot be released with regular 'delete'.");
  }

  //===--- Statement bitfields classes ---===//

  class StmtBitfields {
    friend class ASTStmtReader;
    friend class ASTStmtWriter;
    friend class Stmt;

    /// The statement class.
    unsigned sClass : 8;
  };
  enum { NumStmtBits = 8 };

  class NullStmtBitfields {
    friend class ASTStmtReader;
    friend class ASTStmtWriter;
    friend class NullStmt;

    unsigned : NumStmtBits;

    /// True if the null statement was preceded by an empty macro, e.g:
    /// @code
    ///   #define CALL(x)
    ///   CALL(0);
    /// @endcode
    unsigned HasLeadingEmptyMacro : 1;

    /// The location of the semi-colon.
    SourceLocation SemiLoc;
  };

  class CompoundStmtBitfields {
    friend class ASTStmtReader;
    friend class CompoundStmt;

    unsigned : NumStmtBits;

    /// True if the compound statement has one or more pragmas that set some
    /// floating-point features.
    unsigned HasFPFeatures : 1;

    unsigned NumStmts;
  };

  class LabelStmtBitfields {
    friend class LabelStmt;

    unsigned : NumStmtBits;

    SourceLocation IdentLoc;
  };

  class AttributedStmtBitfields {
    friend class ASTStmtReader;
    friend class AttributedStmt;

    unsigned : NumStmtBits;

    /// Number of attributes.
    unsigned NumAttrs : 32 - NumStmtBits;

    /// The location of the attribute.
    SourceLocation AttrLoc;
  };

  class IfStmtBitfields {
    friend class ASTStmtReader;
    friend class IfStmt;

    unsigned : NumStmtBits;

    /// Whether this is a constexpr if, or a consteval if, or neither.
    unsigned Kind : 3;

    /// True if this if statement has storage for an else statement.
    unsigned HasElse : 1;

    /// True if this if statement has storage for a variable declaration.
    unsigned HasVar : 1;

    /// True if this if statement has storage for an init statement.
    unsigned HasInit : 1;

    /// The location of the "if".
    SourceLocation IfLoc;
  };

  class SwitchStmtBitfields {
    friend class SwitchStmt;

    unsigned : NumStmtBits;

    /// True if the SwitchStmt has storage for an init statement.
    unsigned HasInit : 1;

    /// True if the SwitchStmt has storage for a condition variable.
    unsigned HasVar : 1;

    /// If the SwitchStmt is a switch on an enum value, records whether all
    /// the enum values were covered by CaseStmts.  The coverage information
    /// value is meant to be a hint for possible clients.
    unsigned AllEnumCasesCovered : 1;

    /// The location of the "switch".
    SourceLocation SwitchLoc;
  };

  class WhileStmtBitfields {
    friend class ASTStmtReader;
    friend class WhileStmt;

    unsigned : NumStmtBits;

    /// True if the WhileStmt has storage for a condition variable.
    unsigned HasVar : 1;

    /// The location of the "while".
    SourceLocation WhileLoc;
  };

  class DoStmtBitfields {
    friend class DoStmt;

    unsigned : NumStmtBits;

    /// The location of the "do".
    SourceLocation DoLoc;
  };

  class ForStmtBitfields {
    friend class ForStmt;

    unsigned : NumStmtBits;

    /// The location of the "for".
    SourceLocation ForLoc;
  };

  class GotoStmtBitfields {
    friend class GotoStmt;
    friend class IndirectGotoStmt;

    unsigned : NumStmtBits;

    /// The location of the "goto".
    SourceLocation GotoLoc;
  };

  class ContinueStmtBitfields {
    friend class ContinueStmt;

    unsigned : NumStmtBits;

    /// The location of the "continue".
    SourceLocation ContinueLoc;
  };

  class BreakStmtBitfields {
    friend class BreakStmt;

    unsigned : NumStmtBits;

    /// The location of the "break".
    SourceLocation BreakLoc;
  };

  class ReturnStmtBitfields {
    friend class ReturnStmt;

    unsigned : NumStmtBits;

    /// True if this ReturnStmt has storage for an NRVO candidate.
    unsigned HasNRVOCandidate : 1;

    /// The location of the "return".
    SourceLocation RetLoc;
  };

  class SwitchCaseBitfields {
    friend class SwitchCase;
    friend class CaseStmt;

    unsigned : NumStmtBits;

    /// Used by CaseStmt to store whether it is a case statement
    /// of the form case LHS ... RHS (a GNU extension).
    unsigned CaseStmtIsGNURange : 1;

    /// The location of the "case" or "default" keyword.
    SourceLocation KeywordLoc;
  };

  //===--- Expression bitfields classes ---===//

  class ExprBitfields {
    friend class ASTStmtReader; // deserialization
    friend class AtomicExpr; // ctor
    friend class BlockDeclRefExpr; // ctor
    friend class CallExpr; // ctor
    friend class CXXConstructExpr; // ctor
    friend class CXXDependentScopeMemberExpr; // ctor
    friend class CXXNewExpr; // ctor
    friend class CXXUnresolvedConstructExpr; // ctor
    friend class DeclRefExpr; // computeDependence
    friend class DependentScopeDeclRefExpr; // ctor
    friend class DesignatedInitExpr; // ctor
    friend class Expr;
    friend class InitListExpr; // ctor
    friend class ObjCArrayLiteral; // ctor
    friend class ObjCDictionaryLiteral; // ctor
    friend class ObjCMessageExpr; // ctor
    friend class OffsetOfExpr; // ctor
    friend class OpaqueValueExpr; // ctor
    friend class OverloadExpr; // ctor
    friend class ParenListExpr; // ctor
    friend class PseudoObjectExpr; // ctor
    friend class ShuffleVectorExpr; // ctor

    unsigned : NumStmtBits;

    unsigned ValueKind : 2;
    unsigned ObjectKind : 3;
    unsigned /*ExprDependence*/ Dependent : llvm::BitWidth<ExprDependence>;
  };
  enum { NumExprBits = NumStmtBits + 5 + llvm::BitWidth<ExprDependence> };

  class ConstantExprBitfields {
    friend class ASTStmtReader;
    friend class ASTStmtWriter;
    friend class ConstantExpr;

    unsigned : NumExprBits;

    /// The kind of result that is tail-allocated.
    unsigned ResultKind : 2;

    /// The kind of Result as defined by APValue::Kind.
    unsigned APValueKind : 4;

    /// When ResultKind == RSK_Int64, true if the tail-allocated integer is
    /// unsigned.
    unsigned IsUnsigned : 1;

    /// When ResultKind == RSK_Int64. the BitWidth of the tail-allocated
    /// integer. 7 bits because it is the minimal number of bits to represent a
    /// value from 0 to 64 (the size of the tail-allocated integer).
    unsigned BitWidth : 7;

    /// When ResultKind == RSK_APValue, true if the ASTContext will cleanup the
    /// tail-allocated APValue.
    unsigned HasCleanup : 1;

    /// True if this ConstantExpr was created for immediate invocation.
    unsigned IsImmediateInvocation : 1;
  };

  class PredefinedExprBitfields {
    friend class ASTStmtReader;
    friend class PredefinedExpr;

    unsigned : NumExprBits;

    /// The kind of this PredefinedExpr. One of the enumeration values
    /// in PredefinedExpr::IdentKind.
    unsigned Kind : 4;

    /// True if this PredefinedExpr has a trailing "StringLiteral *"
    /// for the predefined identifier.
    unsigned HasFunctionName : 1;

    /// The location of this PredefinedExpr.
    SourceLocation Loc;
  };

  class DeclRefExprBitfields {
    friend class ASTStmtReader; // deserialization
    friend class DeclRefExpr;

    unsigned : NumExprBits;

    unsigned HasQualifier : 1;
    unsigned HasTemplateKWAndArgsInfo : 1;
    unsigned HasFoundDecl : 1;
    unsigned HadMultipleCandidates : 1;
    unsigned RefersToEnclosingVariableOrCapture : 1;
    unsigned NonOdrUseReason : 2;

    /// The location of the declaration name itself.
    SourceLocation Loc;
  };


  class FloatingLiteralBitfields {
    friend class FloatingLiteral;

    unsigned : NumExprBits;

    unsigned Semantics : 3; // Provides semantics for APFloat construction
    unsigned IsExact : 1;
  };

  class StringLiteralBitfields {
    friend class ASTStmtReader;
    friend class StringLiteral;

    unsigned : NumExprBits;

    /// The kind of this string literal.
    /// One of the enumeration values of StringLiteral::StringKind.
    unsigned Kind : 3;

    /// The width of a single character in bytes. Only values of 1, 2,
    /// and 4 bytes are supported. StringLiteral::mapCharByteWidth maps
    /// the target + string kind to the appropriate CharByteWidth.
    unsigned CharByteWidth : 3;

    unsigned IsPascal : 1;

    /// The number of concatenated token this string is made of.
    /// This is the number of trailing SourceLocation.
    unsigned NumConcatenated;
  };

  class CharacterLiteralBitfields {
    friend class CharacterLiteral;

    unsigned : NumExprBits;

    unsigned Kind : 3;
  };

  class UnaryOperatorBitfields {
    friend class UnaryOperator;

    unsigned : NumExprBits;

    unsigned Opc : 5;
    unsigned CanOverflow : 1;
    //
    /// This is only meaningful for operations on floating point
    /// types when additional values need to be in trailing storage.
    /// It is 0 otherwise.
    unsigned HasFPFeatures : 1;

    SourceLocation Loc;
  };

  class UnaryExprOrTypeTraitExprBitfields {
    friend class UnaryExprOrTypeTraitExpr;

    unsigned : NumExprBits;

    unsigned Kind : 3;
    unsigned IsType : 1; // true if operand is a type, false if an expression.
  };

  class ArrayOrMatrixSubscriptExprBitfields {
    friend class ArraySubscriptExpr;
    friend class MatrixSubscriptExpr;

    unsigned : NumExprBits;

    SourceLocation RBracketLoc;
  };

  class CallExprBitfields {
    friend class CallExpr;

    unsigned : NumExprBits;

    unsigned NumPreArgs : 1;

    /// True if the callee of the call expression was found using ADL.
    unsigned UsesADL : 1;

    /// True if the call expression has some floating-point features.
    unsigned HasFPFeatures : 1;

    /// Padding used to align OffsetToTrailingObjects to a byte multiple.
    unsigned : 24 - 3 - NumExprBits;

    /// The offset in bytes from the this pointer to the start of the
    /// trailing objects belonging to CallExpr. Intentionally byte sized
    /// for faster access.
    unsigned OffsetToTrailingObjects : 8;
  };
  enum { NumCallExprBits = 32 };

  class MemberExprBitfields {
    friend class ASTStmtReader;
    friend class MemberExpr;

    unsigned : NumExprBits;

    /// IsArrow - True if this is "X->F", false if this is "X.F".
    unsigned IsArrow : 1;

    /// True if this member expression used a nested-name-specifier to
    /// refer to the member, e.g., "x->Base::f", or found its member via
    /// a using declaration.  When true, a MemberExprNameQualifier
    /// structure is allocated immediately after the MemberExpr.
    unsigned HasQualifierOrFoundDecl : 1;

    /// True if this member expression specified a template keyword
    /// and/or a template argument list explicitly, e.g., x->f<int>,
    /// x->template f, x->template f<int>.
    /// When true, an ASTTemplateKWAndArgsInfo structure and its
    /// TemplateArguments (if any) are present.
    unsigned HasTemplateKWAndArgsInfo : 1;

    /// True if this member expression refers to a method that
    /// was resolved from an overloaded set having size greater than 1.
    unsigned HadMultipleCandidates : 1;

    /// Value of type NonOdrUseReason indicating why this MemberExpr does
    /// not constitute an odr-use of the named declaration. Meaningful only
    /// when naming a static member.
    unsigned NonOdrUseReason : 2;

    /// This is the location of the -> or . in the expression.
    SourceLocation OperatorLoc;
  };

  class CastExprBitfields {
    friend class CastExpr;
    friend class ImplicitCastExpr;

    unsigned : NumExprBits;

    unsigned Kind : 7;
    unsigned PartOfExplicitCast : 1; // Only set for ImplicitCastExpr.

    /// True if the call expression has some floating-point features.
    unsigned HasFPFeatures : 1;

    /// The number of CXXBaseSpecifiers in the cast. 14 bits would be enough
    /// here. ([implimits] Direct and indirect base classes [16384]).
    unsigned BasePathSize;
  };

  class BinaryOperatorBitfields {
    friend class BinaryOperator;

    unsigned : NumExprBits;

    unsigned Opc : 6;

    /// This is only meaningful for operations on floating point
    /// types when additional values need to be in trailing storage.
    /// It is 0 otherwise.
    unsigned HasFPFeatures : 1;

    SourceLocation OpLoc;
  };

  class InitListExprBitfields {
    friend class InitListExpr;

    unsigned : NumExprBits;

    /// Whether this initializer list originally had a GNU array-range
    /// designator in it. This is a temporary marker used by CodeGen.
    unsigned HadArrayRangeDesignator : 1;
  };

  class ParenListExprBitfields {
    friend class ASTStmtReader;
    friend class ParenListExpr;

    unsigned : NumExprBits;

    /// The number of expressions in the paren list.
    unsigned NumExprs;
  };

  class GenericSelectionExprBitfields {
    friend class ASTStmtReader;
    friend class GenericSelectionExpr;

    unsigned : NumExprBits;

    /// The location of the "_Generic".
    SourceLocation GenericLoc;
  };

  class PseudoObjectExprBitfields {
    friend class ASTStmtReader; // deserialization
    friend class PseudoObjectExpr;

    unsigned : NumExprBits;

    // These don't need to be particularly wide, because they're
    // strictly limited by the forms of expressions we permit.
    unsigned NumSubExprs : 8;
    unsigned ResultIndex : 32 - 8 - NumExprBits;
  };

  class SourceLocExprBitfields {
    friend class ASTStmtReader;
    friend class SourceLocExpr;

    unsigned : NumExprBits;

    /// The kind of source location builtin represented by the SourceLocExpr.
    /// Ex. __builtin_LINE, __builtin_FUNCTION, etc.
    unsigned Kind : 3;
  };

  class StmtExprBitfields {
    friend class ASTStmtReader;
    friend class StmtExpr;

    unsigned : NumExprBits;

    /// The number of levels of template parameters enclosing this statement
    /// expression. Used to determine if a statement expression remains
    /// dependent after instantiation.
    unsigned TemplateDepth;
  };

  //===--- C++ Expression bitfields classes ---===//

  class CXXOperatorCallExprBitfields {
    friend class ASTStmtReader;
    friend class CXXOperatorCallExpr;

    unsigned : NumCallExprBits;

    /// The kind of this overloaded operator. One of the enumerator
    /// value of OverloadedOperatorKind.
    unsigned OperatorKind : 6;
  };

  class CXXRewrittenBinaryOperatorBitfields {
    friend class ASTStmtReader;
    friend class CXXRewrittenBinaryOperator;

    unsigned : NumCallExprBits;

    unsigned IsReversed : 1;
  };

  class CXXBoolLiteralExprBitfields {
    friend class CXXBoolLiteralExpr;

    unsigned : NumExprBits;

    /// The value of the boolean literal.
    unsigned Value : 1;

    /// The location of the boolean literal.
    SourceLocation Loc;
  };

  class CXXNullPtrLiteralExprBitfields {
    friend class CXXNullPtrLiteralExpr;

    unsigned : NumExprBits;

    /// The location of the null pointer literal.
    SourceLocation Loc;
  };

  class CXXThisExprBitfields {
    friend class CXXThisExpr;

    unsigned : NumExprBits;

    /// Whether this is an implicit "this".
    unsigned IsImplicit : 1;

    /// The location of the "this".
    SourceLocation Loc;
  };

  class CXXThrowExprBitfields {
    friend class ASTStmtReader;
    friend class CXXThrowExpr;

    unsigned : NumExprBits;

    /// Whether the thrown variable (if any) is in scope.
    unsigned IsThrownVariableInScope : 1;

    /// The location of the "throw".
    SourceLocation ThrowLoc;
  };

  class CXXDefaultArgExprBitfields {
    friend class ASTStmtReader;
    friend class CXXDefaultArgExpr;

    unsigned : NumExprBits;

    /// The location where the default argument expression was used.
    SourceLocation Loc;
  };

  class CXXDefaultInitExprBitfields {
    friend class ASTStmtReader;
    friend class CXXDefaultInitExpr;

    unsigned : NumExprBits;

    /// The location where the default initializer expression was used.
    SourceLocation Loc;
  };

  class CXXScalarValueInitExprBitfields {
    friend class ASTStmtReader;
    friend class CXXScalarValueInitExpr;

    unsigned : NumExprBits;

    SourceLocation RParenLoc;
  };

  class CXXNewExprBitfields {
    friend class ASTStmtReader;
    friend class ASTStmtWriter;
    friend class CXXNewExpr;

    unsigned : NumExprBits;

    /// Was the usage ::new, i.e. is the global new to be used?
    unsigned IsGlobalNew : 1;

    /// Do we allocate an array? If so, the first trailing "Stmt *" is the
    /// size expression.
    unsigned IsArray : 1;

    /// Should the alignment be passed to the allocation function?
    unsigned ShouldPassAlignment : 1;

    /// If this is an array allocation, does the usual deallocation
    /// function for the allocated type want to know the allocated size?
    unsigned UsualArrayDeleteWantsSize : 1;

    /// What kind of initializer do we have? Could be none, parens, or braces.
    /// In storage, we distinguish between "none, and no initializer expr", and
    /// "none, but an implicit initializer expr".
    unsigned StoredInitializationStyle : 2;

    /// True if the allocated type was expressed as a parenthesized type-id.
    unsigned IsParenTypeId : 1;

    /// The number of placement new arguments.
    unsigned NumPlacementArgs;
  };

  class CXXDeleteExprBitfields {
    friend class ASTStmtReader;
    friend class CXXDeleteExpr;

    unsigned : NumExprBits;

    /// Is this a forced global delete, i.e. "::delete"?
    unsigned GlobalDelete : 1;

    /// Is this the array form of delete, i.e. "delete[]"?
    unsigned ArrayForm : 1;

    /// ArrayFormAsWritten can be different from ArrayForm if 'delete' is
    /// applied to pointer-to-array type (ArrayFormAsWritten will be false
    /// while ArrayForm will be true).
    unsigned ArrayFormAsWritten : 1;

    /// Does the usual deallocation function for the element type require
    /// a size_t argument?
    unsigned UsualArrayDeleteWantsSize : 1;

    /// Location of the expression.
    SourceLocation Loc;
  };

  class TypeTraitExprBitfields {
    friend class ASTStmtReader;
    friend class ASTStmtWriter;
    friend class TypeTraitExpr;

    unsigned : NumExprBits;

    /// The kind of type trait, which is a value of a TypeTrait enumerator.
    unsigned Kind : 8;

    /// If this expression is not value-dependent, this indicates whether
    /// the trait evaluated true or false.
    unsigned Value : 1;

    /// The number of arguments to this type trait. According to [implimits]
    /// 8 bits would be enough, but we require (and test for) at least 16 bits
    /// to mirror FunctionType.
    unsigned NumArgs;
  };

  class DependentScopeDeclRefExprBitfields {
    friend class ASTStmtReader;
    friend class ASTStmtWriter;
    friend class DependentScopeDeclRefExpr;

    unsigned : NumExprBits;

    /// Whether the name includes info for explicit template
    /// keyword and arguments.
    unsigned HasTemplateKWAndArgsInfo : 1;
  };

  class CXXConstructExprBitfields {
    friend class ASTStmtReader;
    friend class CXXConstructExpr;

    unsigned : NumExprBits;

    unsigned Elidable : 1;
    unsigned HadMultipleCandidates : 1;
    unsigned ListInitialization : 1;
    unsigned StdInitListInitialization : 1;
    unsigned ZeroInitialization : 1;
    unsigned ConstructionKind : 3;

    SourceLocation Loc;
  };

  class ExprWithCleanupsBitfields {
    friend class ASTStmtReader; // deserialization
    friend class ExprWithCleanups;

    unsigned : NumExprBits;

    // When false, it must not have side effects.
    unsigned CleanupsHaveSideEffects : 1;

    unsigned NumObjects : 32 - 1 - NumExprBits;
  };

  class CXXUnresolvedConstructExprBitfields {
    friend class ASTStmtReader;
    friend class CXXUnresolvedConstructExpr;

    unsigned : NumExprBits;

    /// The number of arguments used to construct the type.
    unsigned NumArgs;
  };

  class CXXDependentScopeMemberExprBitfields {
    friend class ASTStmtReader;
    friend class CXXDependentScopeMemberExpr;

    unsigned : NumExprBits;

    /// Whether this member expression used the '->' operator or
    /// the '.' operator.
    unsigned IsArrow : 1;

    /// Whether this member expression has info for explicit template
    /// keyword and arguments.
    unsigned HasTemplateKWAndArgsInfo : 1;

    /// See getFirstQualifierFoundInScope() and the comment listing
    /// the trailing objects.
    unsigned HasFirstQualifierFoundInScope : 1;

    /// The location of the '->' or '.' operator.
    SourceLocation OperatorLoc;
  };

  class OverloadExprBitfields {
    friend class ASTStmtReader;
    friend class OverloadExpr;

    unsigned : NumExprBits;

    /// Whether the name includes info for explicit template
    /// keyword and arguments.
    unsigned HasTemplateKWAndArgsInfo : 1;

    /// Padding used by the derived classes to store various bits. If you
    /// need to add some data here, shrink this padding and add your data
    /// above. NumOverloadExprBits also needs to be updated.
    unsigned : 32 - NumExprBits - 1;

    /// The number of results.
    unsigned NumResults;
  };
  enum { NumOverloadExprBits = NumExprBits + 1 };

  class UnresolvedLookupExprBitfields {
    friend class ASTStmtReader;
    friend class UnresolvedLookupExpr;

    unsigned : NumOverloadExprBits;

    /// True if these lookup results should be extended by
    /// argument-dependent lookup if this is the operand of a function call.
    unsigned RequiresADL : 1;

    /// True if these lookup results are overloaded.  This is pretty trivially
    /// rederivable if we urgently need to kill this field.
    unsigned Overloaded : 1;
  };
  static_assert(sizeof(UnresolvedLookupExprBitfields) <= 4,
                "UnresolvedLookupExprBitfields must be <= than 4 bytes to"
                "avoid trashing OverloadExprBitfields::NumResults!");

  class UnresolvedMemberExprBitfields {
    friend class ASTStmtReader;
    friend class UnresolvedMemberExpr;

    unsigned : NumOverloadExprBits;

    /// Whether this member expression used the '->' operator or
    /// the '.' operator.
    unsigned IsArrow : 1;

    /// Whether the lookup results contain an unresolved using declaration.
    unsigned HasUnresolvedUsing : 1;
  };
  static_assert(sizeof(UnresolvedMemberExprBitfields) <= 4,
                "UnresolvedMemberExprBitfields must be <= than 4 bytes to"
                "avoid trashing OverloadExprBitfields::NumResults!");

  class CXXNoexceptExprBitfields {
    friend class ASTStmtReader;
    friend class CXXNoexceptExpr;

    unsigned : NumExprBits;

    unsigned Value : 1;
  };

  class SubstNonTypeTemplateParmExprBitfields {
    friend class ASTStmtReader;
    friend class SubstNonTypeTemplateParmExpr;

    unsigned : NumExprBits;

    /// The location of the non-type template parameter reference.
    SourceLocation NameLoc;
  };

  class LambdaExprBitfields {
    friend class ASTStmtReader;
    friend class ASTStmtWriter;
    friend class LambdaExpr;

    unsigned : NumExprBits;

    /// The default capture kind, which is a value of type
    /// LambdaCaptureDefault.
    unsigned CaptureDefault : 2;

    /// Whether this lambda had an explicit parameter list vs. an
    /// implicit (and empty) parameter list.
    unsigned ExplicitParams : 1;

    /// Whether this lambda had the result type explicitly specified.
    unsigned ExplicitResultType : 1;

    /// The number of captures.
    unsigned NumCaptures : 16;
  };

  class RequiresExprBitfields {
    friend class ASTStmtReader;
    friend class ASTStmtWriter;
    friend class RequiresExpr;

    unsigned : NumExprBits;

    unsigned IsSatisfied : 1;
    SourceLocation RequiresKWLoc;
  };

  //===--- C++ Coroutines TS bitfields classes ---===//

  class CoawaitExprBitfields {
    friend class CoawaitExpr;

    unsigned : NumExprBits;

    unsigned IsImplicit : 1;
  };

  //===--- Obj-C Expression bitfields classes ---===//

  class ObjCIndirectCopyRestoreExprBitfields {
    friend class ObjCIndirectCopyRestoreExpr;

    unsigned : NumExprBits;

    unsigned ShouldCopy : 1;
  };

  //===--- Clang Extensions bitfields classes ---===//

  class OpaqueValueExprBitfields {
    friend class ASTStmtReader;
    friend class OpaqueValueExpr;

    unsigned : NumExprBits;

    /// The OVE is a unique semantic reference to its source expression if this
    /// bit is set to true.
    unsigned IsUnique : 1;

    SourceLocation Loc;
  };

  union {
    // Same order as in StmtNodes.td.
    // Statements
    StmtBitfields StmtBits;
    NullStmtBitfields NullStmtBits;
    CompoundStmtBitfields CompoundStmtBits;
    LabelStmtBitfields LabelStmtBits;
    AttributedStmtBitfields AttributedStmtBits;
    IfStmtBitfields IfStmtBits;
    SwitchStmtBitfields SwitchStmtBits;
    WhileStmtBitfields WhileStmtBits;
    DoStmtBitfields DoStmtBits;
    ForStmtBitfields ForStmtBits;
    GotoStmtBitfields GotoStmtBits;
    ContinueStmtBitfields ContinueStmtBits;
    BreakStmtBitfields BreakStmtBits;
    ReturnStmtBitfields ReturnStmtBits;
    SwitchCaseBitfields SwitchCaseBits;

    // Expressions
    ExprBitfields ExprBits;
    ConstantExprBitfields ConstantExprBits;
    PredefinedExprBitfields PredefinedExprBits;
    DeclRefExprBitfields DeclRefExprBits;
    FloatingLiteralBitfields FloatingLiteralBits;
    StringLiteralBitfields StringLiteralBits;
    CharacterLiteralBitfields CharacterLiteralBits;
    UnaryOperatorBitfields UnaryOperatorBits;
    UnaryExprOrTypeTraitExprBitfields UnaryExprOrTypeTraitExprBits;
    ArrayOrMatrixSubscriptExprBitfields ArrayOrMatrixSubscriptExprBits;
    CallExprBitfields CallExprBits;
    MemberExprBitfields MemberExprBits;
    CastExprBitfields CastExprBits;
    BinaryOperatorBitfields BinaryOperatorBits;
    InitListExprBitfields InitListExprBits;
    ParenListExprBitfields ParenListExprBits;
    GenericSelectionExprBitfields GenericSelectionExprBits;
    PseudoObjectExprBitfields PseudoObjectExprBits;
    SourceLocExprBitfields SourceLocExprBits;

    // GNU Extensions.
    StmtExprBitfields StmtExprBits;

    // C++ Expressions
    CXXOperatorCallExprBitfields CXXOperatorCallExprBits;
    CXXRewrittenBinaryOperatorBitfields CXXRewrittenBinaryOperatorBits;
    CXXBoolLiteralExprBitfields CXXBoolLiteralExprBits;
    CXXNullPtrLiteralExprBitfields CXXNullPtrLiteralExprBits;
    CXXThisExprBitfields CXXThisExprBits;
    CXXThrowExprBitfields CXXThrowExprBits;
    CXXDefaultArgExprBitfields CXXDefaultArgExprBits;
    CXXDefaultInitExprBitfields CXXDefaultInitExprBits;
    CXXScalarValueInitExprBitfields CXXScalarValueInitExprBits;
    CXXNewExprBitfields CXXNewExprBits;
    CXXDeleteExprBitfields CXXDeleteExprBits;
    TypeTraitExprBitfields TypeTraitExprBits;
    DependentScopeDeclRefExprBitfields DependentScopeDeclRefExprBits;
    CXXConstructExprBitfields CXXConstructExprBits;
    ExprWithCleanupsBitfields ExprWithCleanupsBits;
    CXXUnresolvedConstructExprBitfields CXXUnresolvedConstructExprBits;
    CXXDependentScopeMemberExprBitfields CXXDependentScopeMemberExprBits;
    OverloadExprBitfields OverloadExprBits;
    UnresolvedLookupExprBitfields UnresolvedLookupExprBits;
    UnresolvedMemberExprBitfields UnresolvedMemberExprBits;
    CXXNoexceptExprBitfields CXXNoexceptExprBits;
    SubstNonTypeTemplateParmExprBitfields SubstNonTypeTemplateParmExprBits;
    LambdaExprBitfields LambdaExprBits;
    RequiresExprBitfields RequiresExprBits;

    // C++ Coroutines TS expressions
    CoawaitExprBitfields CoawaitBits;

    // Obj-C Expressions
    ObjCIndirectCopyRestoreExprBitfields ObjCIndirectCopyRestoreExprBits;

    // Clang Extensions
    OpaqueValueExprBitfields OpaqueValueExprBits;
  };

public:
  // Only allow allocation of Stmts using the allocator in ASTContext
  // or by doing a placement new.
  void* operator new(size_t bytes, const ASTContext& C,
                     unsigned alignment = 8);

  void* operator new(size_t bytes, const ASTContext* C,
                     unsigned alignment = 8) {
    return operator new(bytes, *C, alignment);
  }

  void *operator new(size_t bytes, void *mem) noexcept { return mem; }

  void operator delete(void *, const ASTContext &, unsigned) noexcept {}
  void operator delete(void *, const ASTContext *, unsigned) noexcept {}
  void operator delete(void *, size_t) noexcept {}
  void operator delete(void *, void *) noexcept {}

public:
  /// A placeholder type used to construct an empty shell of a
  /// type, that will be filled in later (e.g., by some
  /// de-serialization).
  struct EmptyShell {};

  /// The likelihood of a branch being taken.
  enum Likelihood {
    LH_Unlikely = -1, ///< Branch has the [[unlikely]] attribute.
    LH_None,          ///< No attribute set or branches of the IfStmt have
                      ///< the same attribute.
    LH_Likely         ///< Branch has the [[likely]] attribute.
  };

protected:
  /// Iterator for iterating over Stmt * arrays that contain only T *.
  ///
  /// This is needed because AST nodes use Stmt* arrays to store
  /// references to children (to be compatible with StmtIterator).
  template<typename T, typename TPtr = T *, typename StmtPtr = Stmt *>
  struct CastIterator
      : llvm::iterator_adaptor_base<CastIterator<T, TPtr, StmtPtr>, StmtPtr *,
                                    std::random_access_iterator_tag, TPtr> {
    using Base = typename CastIterator::iterator_adaptor_base;

    CastIterator() : Base(nullptr) {}
    CastIterator(StmtPtr *I) : Base(I) {}

    typename Base::value_type operator*() const {
      return cast_or_null<T>(*this->I);
    }
  };

  /// Const iterator for iterating over Stmt * arrays that contain only T *.
  template <typename T>
  using ConstCastIterator = CastIterator<T, const T *const, const Stmt *const>;

  using ExprIterator = CastIterator<Expr>;
  using ConstExprIterator = ConstCastIterator<Expr>;

private:
  /// Whether statistic collection is enabled.
  static bool StatisticsEnabled;

protected:
  /// Construct an empty statement.
  explicit Stmt(StmtClass SC, EmptyShell) : Stmt(SC) {}

public:
  Stmt() = delete;
  Stmt(const Stmt &) = delete;
  Stmt(Stmt &&) = delete;
  Stmt &operator=(const Stmt &) = delete;
  Stmt &operator=(Stmt &&) = delete;

  Stmt(StmtClass SC) {
    static_assert(sizeof(*this) <= 8,
                  "changing bitfields changed sizeof(Stmt)");
    static_assert(sizeof(*this) % alignof(void *) == 0,
                  "Insufficient alignment!");
    StmtBits.sClass = SC;
    if (StatisticsEnabled) Stmt::addStmtClass(SC);
  }

  StmtClass getStmtClass() const {
    return static_cast<StmtClass>(StmtBits.sClass);
  }

  const char *getStmtClassName() const;

  /// SourceLocation tokens are not useful in isolation - they are low level
  /// value objects created/interpreted by SourceManager. We assume AST
  /// clients will have a pointer to the respective SourceManager.
  SourceRange getSourceRange() const LLVM_READONLY;
  SourceLocation getBeginLoc() const LLVM_READONLY;
  SourceLocation getEndLoc() const LLVM_READONLY;

  // global temp stats (until we have a per-module visitor)
  static void addStmtClass(const StmtClass s);
  static void EnableStatistics();
  static void PrintStats();

  /// \returns the likelihood of a set of attributes.
  static Likelihood getLikelihood(ArrayRef<const Attr *> Attrs);

  /// \returns the likelihood of a statement.
  static Likelihood getLikelihood(const Stmt *S);

  /// \returns the likelihood attribute of a statement.
  static const Attr *getLikelihoodAttr(const Stmt *S);

  /// \returns the likelihood of the 'then' branch of an 'if' statement. The
  /// 'else' branch is required to determine whether both branches specify the
  /// same likelihood, which affects the result.
  static Likelihood getLikelihood(const Stmt *Then, const Stmt *Else);

  /// \returns whether the likelihood of the branches of an if statement are
  /// conflicting. When the first element is \c true there's a conflict and
  /// the Attr's are the conflicting attributes of the Then and Else Stmt.
  static std::tuple<bool, const Attr *, const Attr *>
  determineLikelihoodConflict(const Stmt *Then, const Stmt *Else);

  /// Dumps the specified AST fragment and all subtrees to
  /// \c llvm::errs().
  void dump() const;
  void dump(raw_ostream &OS, const ASTContext &Context) const;

  /// \return Unique reproducible object identifier
  int64_t getID(const ASTContext &Context) const;

  /// dumpColor - same as dump(), but forces color highlighting.
  void dumpColor() const;

  /// dumpPretty/printPretty - These two methods do a "pretty print" of the AST
  /// back to its original source language syntax.
  void dumpPretty(const ASTContext &Context) const;
  //void printPretty(raw_ostream &OS, PrinterHelper *Helper,
  //                 const PrintingPolicy &Policy, unsigned Indentation = 0,
  //                 StringRef NewlineSymbol = "\n",
  //                 const ASTContext *Context = nullptr) const;
  //void printPrettyControlled(raw_ostream &OS, PrinterHelper *Helper,
  //                           const PrintingPolicy &Policy,
  //                           unsigned Indentation = 0,
  //                           StringRef NewlineSymbol = "\n",
  //                           const ASTContext *Context = nullptr) const;

  ///// Pretty-prints in JSON format.
  //void printJson(raw_ostream &Out, PrinterHelper *Helper,
  //               const PrintingPolicy &Policy, bool AddQuotes) const;

  /// viewAST - Visualize an AST rooted at this Stmt* using GraphViz.  Only
  ///   works on systems with GraphViz (Mac OS X) or dot+gv installed.
  void viewAST() const;

  /// Skip no-op (attributed, compound) container stmts and skip captured
  /// stmt at the top, if \a IgnoreCaptured is true.
  Stmt *IgnoreContainers(bool IgnoreCaptured = false);
  const Stmt *IgnoreContainers(bool IgnoreCaptured = false) const {
    return const_cast<Stmt *>(this)->IgnoreContainers(IgnoreCaptured);
  }

  const Stmt *stripLabelLikeStatements() const;
  Stmt *stripLabelLikeStatements() {
    return const_cast<Stmt*>(
      const_cast<const Stmt*>(this)->stripLabelLikeStatements());
  }

  /// Child Iterators: All subclasses must implement 'children'
  /// to permit easy iteration over the substatements/subexpressions of an
  /// AST node.  This permits easy iteration over all nodes in the AST.
  using child_iterator = StmtIterator;
  using const_child_iterator = ConstStmtIterator;

  using child_range = llvm::iterator_range<child_iterator>;
  using const_child_range = llvm::iterator_range<const_child_iterator>;

  child_range children();

  const_child_range children() const {
    auto Children = const_cast<Stmt *>(this)->children();
    return const_child_range(Children.begin(), Children.end());
  }

  child_iterator child_begin() { return children().begin(); }
  child_iterator child_end() { return children().end(); }

  const_child_iterator child_begin() const { return children().begin(); }
  const_child_iterator child_end() const { return children().end(); }

  /// Produce a unique representation of the given statement.
  ///
  /// \param ID once the profiling operation is complete, will contain
  /// the unique representation of the given statement.
  ///
  /// \param Context the AST context in which the statement resides
  ///
  /// \param Canonical whether the profile should be based on the canonical
  /// representation of this statement (e.g., where non-type template
  /// parameters are identified by index/level rather than their
  /// declaration pointers) or the exact representation of the statement as
  /// written in the source.
  void Profile(llvm::FoldingSetNodeID &ID, const ASTContext &Context,
               bool Canonical) const;

  /// Calculate a unique representation for a statement that is
  /// stable across compiler invocations.
  ///
  /// \param ID profile information will be stored in ID.
  ///
  /// \param Hash an ODRHash object which will be called where pointers would
  /// have been used in the Profile function.
  void ProcessODRHash(llvm::FoldingSetNodeID &ID, ODRHash& Hash) const;
};

class ValueStmt : public Stmt {
protected:
  using Stmt::Stmt;

public:
  const Expr *getExprStmt() const;
  Expr *getExprStmt() {
    const ValueStmt *ConstThis = this;
    return const_cast<Expr*>(ConstThis->getExprStmt());
  }

  static bool classof(const Stmt *T) {
    return T->getStmtClass() >= firstValueStmtConstant &&
           T->getStmtClass() <= lastValueStmtConstant;
  }
};

/// This represents one expression.  Note that Expr's are subclasses of Stmt.
/// This allows an expression to be transparently used any place a Stmt is
/// required.
class Expr : public ValueStmt {
  QualType TR;

public:
  Expr() = delete;
  Expr(const Expr&) = delete;
  Expr(Expr &&) = delete;
  Expr &operator=(const Expr&) = delete;
  Expr &operator=(Expr&&) = delete;

protected:
  Expr(StmtClass SC, QualType T, ExprValueKind VK, ExprObjectKind OK)
      : ValueStmt(SC) {
    setType(T);
  }

  /// Construct an empty expression.
  explicit Expr(StmtClass SC, EmptyShell) : ValueStmt(SC) { }

  /// Each concrete expr subclass is expected to compute its dependence and call
  /// this in the constructor.
  void setDependence(ExprDependence Deps) {}
  friend class ASTImporter; // Sets dependence dircetly.
  friend class ASTStmtReader; // Sets dependence dircetly.

public:
  QualType getType() const { return TR; }
  void setType(QualType t) {
    // In C++, the type of an expression is always adjusted so that it
    // will not have reference type (C++ [expr]p6). Use
    // QualType::getNonReferenceType() to retrieve the non-reference
    // type. Additionally, inspect Expr::isLvalue to determine whether
    // an expression that is adjusted in this manner should be
    // considered an lvalue.
    assert((t.isNull() || !t->isReferenceType()) &&
           "Expressions can't have reference type");

    TR = t;
  }

  ExprDependence getDependence() const;

  bool isValueDependent() const {
    return static_cast<bool>(getDependence() & ExprDependence::Value);
  }
  bool isTypeDependent() const {
    return static_cast<bool>(getDependence() & ExprDependence::Type);
  }
  bool isInstantiationDependent() const {
    return static_cast<bool>(getDependence() & ExprDependence::Instantiation);
  }
  bool containsUnexpandedParameterPack() const {
    return static_cast<bool>(getDependence() & ExprDependence::UnexpandedPack);
  }
  bool containsErrors() const {
    return static_cast<bool>(getDependence() & ExprDependence::Error);
  }
  SourceLocation getExprLoc() const LLVM_READONLY;
  bool isReadIfDiscardedInCPlusPlus11() const;
  bool isUnusedResultAWarning(const Expr *&WarnExpr, SourceLocation &Loc,
                              SourceRange &R1, SourceRange &R2,
                              ASTContext &Ctx) const;
  bool isLValue() const { return getValueKind() == VK_LValue; }
  bool isPRValue() const { return getValueKind() == VK_PRValue; }
  bool isXValue() const { return getValueKind() == VK_XValue; }
  bool isGLValue() const { return getValueKind() != VK_PRValue; }

  enum LValueClassification {
    LV_Valid,
    LV_NotObjectType,
    LV_IncompleteVoidType,
    LV_DuplicateVectorComponents,
    LV_InvalidExpression,
    LV_InvalidMessageExpression,
    LV_MemberFunction,
    LV_SubObjCPropertySetting,
    LV_ClassTemporary,
    LV_ArrayTemporary
  };
  /// Reasons why an expression might not be an l-value.
  LValueClassification ClassifyLValue(ASTContext &Ctx) const;

  enum isModifiableLvalueResult {
    MLV_Valid,
    MLV_NotObjectType,
    MLV_IncompleteVoidType,
    MLV_DuplicateVectorComponents,
    MLV_InvalidExpression,
    MLV_LValueCast,           // Specialized form of MLV_InvalidExpression.
    MLV_IncompleteType,
    MLV_ConstQualified,
    MLV_ConstQualifiedField,
    MLV_ConstAddrSpace,
    MLV_ArrayType,
    MLV_NoSetterProperty,
    MLV_MemberFunction,
    MLV_SubObjCPropertySetting,
    MLV_InvalidMessageExpression,
    MLV_ClassTemporary,
    MLV_ArrayTemporary
  };
  /// isModifiableLvalue - C99 6.3.2.1: an lvalue that does not have array type,
  /// does not have an incomplete type, does not have a const-qualified type,
  /// and if it is a structure or union, does not have any member (including,
  /// recursively, any member or element of all contained aggregates or unions)
  /// with a const-qualified type.
  ///
  /// \param Loc [in,out] - A source location which *may* be filled
  /// in with the location of the expression making this a
  /// non-modifiable lvalue, if specified.
  isModifiableLvalueResult
  isModifiableLvalue(ASTContext &Ctx, SourceLocation *Loc = nullptr) const;

  /// The return type of classify(). Represents the C++11 expression
  ///        taxonomy.
  class Classification {
  public:
    /// The various classification results. Most of these mean prvalue.
    enum Kinds {
      CL_LValue,
      CL_XValue,
      CL_Function, // Functions cannot be lvalues in C.
      CL_Void, // Void cannot be an lvalue in C.
      CL_AddressableVoid, // Void expression whose address can be taken in C.
      CL_DuplicateVectorComponents, // A vector shuffle with dupes.
      CL_MemberFunction, // An expression referring to a member function
      CL_SubObjCPropertySetting,
      CL_ClassTemporary, // A temporary of class type, or subobject thereof.
      CL_ArrayTemporary, // A temporary of array type.
      CL_ObjCMessageRValue, // ObjC message is an rvalue
      CL_PRValue // A prvalue for any other reason, of any other type
    };
    /// The results of modification testing.
    enum ModifiableType {
      CM_Untested, // testModifiable was false.
      CM_Modifiable,
      CM_RValue, // Not modifiable because it's an rvalue
      CM_Function, // Not modifiable because it's a function; C++ only
      CM_LValueCast, // Same as CM_RValue, but indicates GCC cast-as-lvalue ext
      CM_NoSetterProperty,// Implicit assignment to ObjC property without setter
      CM_ConstQualified,
      CM_ConstQualifiedField,
      CM_ConstAddrSpace,
      CM_ArrayType,
      CM_IncompleteType
    };

  private:
    friend class Expr;

    unsigned short Kind;
    unsigned short Modifiable;

    explicit Classification(Kinds k, ModifiableType m)
      : Kind(k), Modifiable(m)
    {}

  public:
    Classification() {}

    Kinds getKind() const { return static_cast<Kinds>(Kind); }
    ModifiableType getModifiable() const {
      assert(Modifiable != CM_Untested && "Did not test for modifiability.");
      return static_cast<ModifiableType>(Modifiable);
    }
    bool isLValue() const { return Kind == CL_LValue; }
    bool isXValue() const { return Kind == CL_XValue; }
    bool isGLValue() const { return Kind <= CL_XValue; }
    bool isPRValue() const { return Kind >= CL_Function; }
    bool isRValue() const { return Kind >= CL_XValue; }
    bool isModifiable() const { return getModifiable() == CM_Modifiable; }

    /// Create a simple, modifiably lvalue
    static Classification makeSimpleLValue() {
      return Classification(CL_LValue, CM_Modifiable);
    }

  };
  /// Classify - Classify this expression according to the C++11
  ///        expression taxonomy.
  ///
  /// C++11 defines ([basic.lval]) a new taxonomy of expressions to replace the
  /// old lvalue vs rvalue. This function determines the type of expression this
  /// is. There are three expression types:
  /// - lvalues are classical lvalues as in C++03.
  /// - prvalues are equivalent to rvalues in C++03.
  /// - xvalues are expressions yielding unnamed rvalue references, e.g. a
  ///   function returning an rvalue reference.
  /// lvalues and xvalues are collectively referred to as glvalues, while
  /// prvalues and xvalues together form rvalues.
  Classification Classify(ASTContext &Ctx) const {
    return ClassifyImpl(Ctx, nullptr);
  }

  /// ClassifyModifiable - Classify this expression according to the
  ///        C++11 expression taxonomy, and see if it is valid on the left side
  ///        of an assignment.
  ///
  /// This function extends classify in that it also tests whether the
  /// expression is modifiable (C99 6.3.2.1p1).
  /// \param Loc A source location that might be filled with a relevant location
  ///            if the expression is not modifiable.
  Classification ClassifyModifiable(ASTContext &Ctx, SourceLocation &Loc) const{
    return ClassifyImpl(Ctx, &Loc);
  }

  /// Returns the set of floating point options that apply to this expression.
  /// Only meaningful for operations on floating point values.
  FPOptions getFPFeaturesInEffect(const LangOptions &LO) const;

  /// getValueKindForType - Given a formal return or parameter type,
  /// give its value kind.
  static ExprValueKind getValueKindForType(QualType T) {
    if (const ReferenceType *RT = T->getAs<ReferenceType>())
      return (isa<LValueReferenceType>(RT)
                ? VK_LValue
                : (RT->getPointeeType()->isFunctionType()
                     ? VK_LValue : VK_XValue));
    return VK_PRValue;
  }

  /// getValueKind - The value kind that this expression produces.
  ExprValueKind getValueKind() const;
  /// getObjectKind - The object kind that this expression produces.
  /// Object kinds are meaningful only for expressions that yield an
  /// l-value or x-value.
  ExprObjectKind getObjectKind() const;
  bool isOrdinaryOrBitFieldObject() const {
    ExprObjectKind OK = getObjectKind();
    return (OK == OK_Ordinary || OK == OK_BitField);
  }

  /// setValueKind - Set the value kind produced by this expression.
  void setValueKind(ExprValueKind Cat);

  /// setObjectKind - Set the object kind produced by this expression.
  void setObjectKind(ExprObjectKind Cat);

private:
  Classification ClassifyImpl(ASTContext &Ctx, SourceLocation *Loc) const;

public:

  /// Returns true if this expression is a gl-value that
  /// potentially refers to a bit-field.
  ///
  /// In C++, whether a gl-value refers to a bitfield is essentially
  /// an aspect of the value-kind type system.
  bool refersToBitField() const { return getObjectKind() == OK_BitField; }

  /// If this expression refers to a bit-field, retrieve the
  /// declaration of that bit-field.
  ///
  /// Note that this returns a non-null pointer in subtly different
  /// places than refersToBitField returns true.  In particular, this can
  /// return a non-null pointer even for r-values loaded from
  /// bit-fields, but it will return null for a conditional bit-field.
  FieldDecl *getSourceBitField();

  const FieldDecl *getSourceBitField() const {
    return const_cast<Expr*>(this)->getSourceBitField();
  }

  Decl *getReferencedDeclOfCallee();
  const Decl *getReferencedDeclOfCallee() const {
    return const_cast<Expr*>(this)->getReferencedDeclOfCallee();
  }

  /// If this expression is an l-value for an Objective C
  /// property, find the underlying property reference expression.
  const ObjCPropertyRefExpr *getObjCProperty() const;

  /// Check if this expression is the ObjC 'self' implicit parameter.
  bool isObjCSelfExpr() const;

  /// Returns whether this expression refers to a vector element.
  bool refersToVectorElement() const;

  /// Returns whether this expression refers to a matrix element.
  bool refersToMatrixElement() const {
    return getObjectKind() == OK_MatrixComponent;
  }

  /// Returns whether this expression refers to a global register
  /// variable.
  bool refersToGlobalRegisterVar() const;

  /// Returns whether this expression has a placeholder type.
  bool hasPlaceholderType() const {
    return getType()->isPlaceholderType();
  }

  /// Returns whether this expression has a specific placeholder type.
  bool hasPlaceholderType(BuiltinType::Kind K) const {
    assert(BuiltinType::isPlaceholderTypeKind(K));
    if (const BuiltinType *BT = dyn_cast<BuiltinType>(getType()))
      return BT->getKind() == K;
    return false;
  }

  /// isKnownToHaveBooleanValue - Return true if this is an integer expression
  /// that is known to return 0 or 1.  This happens for _Bool/bool expressions
  /// but also int expressions which are produced by things like comparisons in
  /// C.
  ///
  /// \param Semantic If true, only return true for expressions that are known
  /// to be semantically boolean, which might not be true even for expressions
  /// that are known to evaluate to 0/1. For instance, reading an unsigned
  /// bit-field with width '1' will evaluate to 0/1, but doesn't necessarily
  /// semantically correspond to a bool.
  bool isKnownToHaveBooleanValue(bool Semantic = true) const;

  /// isIntegerConstantExpr - Return the value if this expression is a valid
  /// integer constant expression.  If not a valid i-c-e, return None and fill
  /// in Loc (if specified) with the location of the invalid expression.
  ///
  /// Note: This does not perform the implicit conversions required by C++11
  /// [expr.const]p5.
  Optional<llvm::APSInt> getIntegerConstantExpr(const ASTContext &Ctx,
                                                SourceLocation *Loc = nullptr,
                                                bool isEvaluated = true) const;
  bool isIntegerConstantExpr(const ASTContext &Ctx,
                             SourceLocation *Loc = nullptr) const;

  /// isCXX98IntegralConstantExpr - Return true if this expression is an
  /// integral constant expression in C++98. Can only be used in C++.
  bool isCXX98IntegralConstantExpr(const ASTContext &Ctx) const;

  /// isCXX11ConstantExpr - Return true if this expression is a constant
  /// expression in C++11. Can only be used in C++.
  ///
  /// Note: This does not perform the implicit conversions required by C++11
  /// [expr.const]p5.
  bool isCXX11ConstantExpr(const ASTContext &Ctx, APValue *Result = nullptr,
                           SourceLocation *Loc = nullptr) const;

  /// isPotentialConstantExpr - Return true if this function's definition
  /// might be usable in a constant expression in C++11, if it were marked
  /// constexpr. Return false if the function can never produce a constant
  /// expression, along with diagnostics describing why not.
  static bool isPotentialConstantExpr(const FunctionDecl *FD,
                                      SmallVectorImpl<
                                        PartialDiagnosticAt> &Diags);

  /// isPotentialConstantExprUnevaluted - Return true if this expression might
  /// be usable in a constant expression in C++11 in an unevaluated context, if
  /// it were in function FD marked constexpr. Return false if the function can
  /// never produce a constant expression, along with diagnostics describing
  /// why not.
  static bool isPotentialConstantExprUnevaluated(Expr *E,
                                                 const FunctionDecl *FD,
                                                 SmallVectorImpl<
                                                   PartialDiagnosticAt> &Diags);

  /// isConstantInitializer - Returns true if this expression can be emitted to
  /// IR as a constant, and thus can be used as a constant initializer in C.
  /// If this expression is not constant and Culprit is non-null,
  /// it is used to store the address of first non constant expr.
  bool isConstantInitializer(ASTContext &Ctx, bool ForRef,
                             const Expr **Culprit = nullptr) const;

  /// If this expression is an unambiguous reference to a single declaration,
  /// in the style of __builtin_function_start, return that declaration.  Note
  /// that this may return a non-static member function or field in C++ if this
  /// expression is a member pointer constant.
  const ValueDecl *getAsBuiltinConstantDeclRef(const ASTContext &Context) const;

  /// EvalStatus is a struct with detailed info about an evaluation in progress.
  struct EvalStatus {
    /// Whether the evaluated expression has side effects.
    /// For example, (f() && 0) can be folded, but it still has side effects.
    bool HasSideEffects;

    /// Whether the evaluation hit undefined behavior.
    /// For example, 1.0 / 0.0 can be folded to Inf, but has undefined behavior.
    /// Likewise, INT_MAX + 1 can be folded to INT_MIN, but has UB.
    bool HasUndefinedBehavior;

    /// Diag - If this is non-null, it will be filled in with a stack of notes
    /// indicating why evaluation failed (or why it failed to produce a constant
    /// expression).
    /// If the expression is unfoldable, the notes will indicate why it's not
    /// foldable. If the expression is foldable, but not a constant expression,
    /// the notes will describes why it isn't a constant expression. If the
    /// expression *is* a constant expression, no notes will be produced.
    SmallVectorImpl<PartialDiagnosticAt> *Diag;

    EvalStatus()
        : HasSideEffects(false), HasUndefinedBehavior(false), Diag(nullptr) {}

    // hasSideEffects - Return true if the evaluated expression has
    // side effects.
    bool hasSideEffects() const {
      return HasSideEffects;
    }
  };

  /// EvalResult is a struct with detailed info about an evaluated expression.
  struct EvalResult : EvalStatus {
    /// Val - This is the value the expression can be folded to.
    APValue Val;

    // isGlobalLValue - Return true if the evaluated lvalue expression
    // is global.
    bool isGlobalLValue() const;
  };

  /// EvaluateAsRValue - Return true if this is a constant which we can fold to
  /// an rvalue using any crazy technique (that has nothing to do with language
  /// standards) that we want to, even if the expression has side-effects. If
  /// this function returns true, it returns the folded constant in Result. If
  /// the expression is a glvalue, an lvalue-to-rvalue conversion will be
  /// applied.
  bool EvaluateAsRValue(EvalResult &Result, const ASTContext &Ctx,
                        bool InConstantContext = false) const;

  /// EvaluateAsBooleanCondition - Return true if this is a constant
  /// which we can fold and convert to a boolean condition using
  /// any crazy technique that we want to, even if the expression has
  /// side-effects.
  bool EvaluateAsBooleanCondition(bool &Result, const ASTContext &Ctx,
                                  bool InConstantContext = false) const;

  enum SideEffectsKind {
    SE_NoSideEffects,          ///< Strictly evaluate the expression.
    SE_AllowUndefinedBehavior, ///< Allow UB that we can give a value, but not
                               ///< arbitrary unmodeled side effects.
    SE_AllowSideEffects        ///< Allow any unmodeled side effect.
  };

  /// EvaluateAsInt - Return true if this is a constant which we can fold and
  /// convert to an integer, using any crazy technique that we want to.
  bool EvaluateAsInt(EvalResult &Result, const ASTContext &Ctx,
                     SideEffectsKind AllowSideEffects = SE_NoSideEffects,
                     bool InConstantContext = false) const;

  /// EvaluateAsFloat - Return true if this is a constant which we can fold and
  /// convert to a floating point value, using any crazy technique that we
  /// want to.
  bool EvaluateAsFloat(llvm::APFloat &Result, const ASTContext &Ctx,
                       SideEffectsKind AllowSideEffects = SE_NoSideEffects,
                       bool InConstantContext = false) const;

  /// EvaluateAsFloat - Return true if this is a constant which we can fold and
  /// convert to a fixed point value.
  bool EvaluateAsFixedPoint(EvalResult &Result, const ASTContext &Ctx,
                            SideEffectsKind AllowSideEffects = SE_NoSideEffects,
                            bool InConstantContext = false) const;

  /// isEvaluatable - Call EvaluateAsRValue to see if this expression can be
  /// constant folded without side-effects, but discard the result.
  bool isEvaluatable(const ASTContext &Ctx,
                     SideEffectsKind AllowSideEffects = SE_NoSideEffects) const;

  /// HasSideEffects - This routine returns true for all those expressions
  /// which have any effect other than producing a value. Example is a function
  /// call, volatile variable read, or throwing an exception. If
  /// IncludePossibleEffects is false, this call treats certain expressions with
  /// potential side effects (such as function call-like expressions,
  /// instantiation-dependent expressions, or invocations from a macro) as not
  /// having side effects.
  bool HasSideEffects(const ASTContext &Ctx,
                      bool IncludePossibleEffects = true) const;

  /// Determine whether this expression involves a call to any function
  /// that is not trivial.
  bool hasNonTrivialCall(const ASTContext &Ctx) const;

  /// EvaluateKnownConstInt - Call EvaluateAsRValue and return the folded
  /// integer. This must be called on an expression that constant folds to an
  /// integer.
  llvm::APSInt EvaluateKnownConstInt(
      const ASTContext &Ctx,
      SmallVectorImpl<PartialDiagnosticAt> *Diag = nullptr) const;

  llvm::APSInt EvaluateKnownConstIntCheckOverflow(
      const ASTContext &Ctx,
      SmallVectorImpl<PartialDiagnosticAt> *Diag = nullptr) const;

  void EvaluateForOverflow(const ASTContext &Ctx) const;

  /// EvaluateAsLValue - Evaluate an expression to see if we can fold it to an
  /// lvalue with link time known address, with no side-effects.
  bool EvaluateAsLValue(EvalResult &Result, const ASTContext &Ctx,
                        bool InConstantContext = false) const;

  /// EvaluateAsInitializer - Evaluate an expression as if it were the
  /// initializer of the given declaration. Returns true if the initializer
  /// can be folded to a constant, and produces any relevant notes. In C++11,
  /// notes will be produced if the expression is not a constant expression.
  bool EvaluateAsInitializer(APValue &Result, const ASTContext &Ctx,
                             const VarDecl *VD,
                             SmallVectorImpl<PartialDiagnosticAt> &Notes,
                             bool IsConstantInitializer) const;

  /// EvaluateWithSubstitution - Evaluate an expression as if from the context
  /// of a call to the given function with the given arguments, inside an
  /// unevaluated context. Returns true if the expression could be folded to a
  /// constant.
  bool EvaluateWithSubstitution(APValue &Value, ASTContext &Ctx,
                                const FunctionDecl *Callee,
                                ArrayRef<const Expr*> Args,
                                const Expr *This = nullptr) const;

  enum class ConstantExprKind {
    /// An integer constant expression (an array bound, enumerator, case value,
    /// bit-field width, or similar) or similar.
    Normal,
    /// A non-class template argument. Such a value is only used for mangling,
    /// not for code generation, so can refer to dllimported functions.
    NonClassTemplateArgument,
    /// A class template argument. Such a value is used for code generation.
    ClassTemplateArgument,
    /// An immediate invocation. The destruction of the end result of this
    /// evaluation is not part of the evaluation, but all other temporaries
    /// are destroyed.
    ImmediateInvocation,
  };

  /// Evaluate an expression that is required to be a constant expression. Does
  /// not check the syntactic constraints for C and C++98 constant expressions.
  bool EvaluateAsConstantExpr(
      EvalResult &Result, const ASTContext &Ctx,
      ConstantExprKind Kind = ConstantExprKind::Normal) const;

  /// If the current Expr is a pointer, this will try to statically
  /// determine the number of bytes available where the pointer is pointing.
  /// Returns true if all of the above holds and we were able to figure out the
  /// size, false otherwise.
  ///
  /// \param Type - How to evaluate the size of the Expr, as defined by the
  /// "type" parameter of __builtin_object_size
  bool tryEvaluateObjectSize(uint64_t &Result, ASTContext &Ctx,
                             unsigned Type) const;

  /// If the current Expr is a pointer, this will try to statically
  /// determine the strlen of the string pointed to.
  /// Returns true if all of the above holds and we were able to figure out the
  /// strlen, false otherwise.
  bool tryEvaluateStrLen(uint64_t &Result, ASTContext &Ctx) const;

  /// Enumeration used to describe the kind of Null pointer constant
  /// returned from \c isNullPointerConstant().
  enum NullPointerConstantKind {
    /// Expression is not a Null pointer constant.
    NPCK_NotNull = 0,

    /// Expression is a Null pointer constant built from a zero integer
    /// expression that is not a simple, possibly parenthesized, zero literal.
    /// C++ Core Issue 903 will classify these expressions as "not pointers"
    /// once it is adopted.
    /// http://www.open-std.org/jtc1/sc22/wg21/docs/cwg_active.html#903
    NPCK_ZeroExpression,

    /// Expression is a Null pointer constant built from a literal zero.
    NPCK_ZeroLiteral,

    /// Expression is a C++11 nullptr.
    NPCK_CXX11_nullptr,

    /// Expression is a GNU-style __null constant.
    NPCK_GNUNull
  };

  /// Enumeration used to describe how \c isNullPointerConstant()
  /// should cope with value-dependent expressions.
  enum NullPointerConstantValueDependence {
    /// Specifies that the expression should never be value-dependent.
    NPC_NeverValueDependent = 0,

    /// Specifies that a value-dependent expression of integral or
    /// dependent type should be considered a null pointer constant.
    NPC_ValueDependentIsNull,

    /// Specifies that a value-dependent expression should be considered
    /// to never be a null pointer constant.
    NPC_ValueDependentIsNotNull
  };

  /// isNullPointerConstant - C99 6.3.2.3p3 - Test if this reduces down to
  /// a Null pointer constant. The return value can further distinguish the
  /// kind of NULL pointer constant that was detected.
  NullPointerConstantKind isNullPointerConstant(
      ASTContext &Ctx,
      NullPointerConstantValueDependence NPC) const;

  /// isOBJCGCCandidate - Return true if this expression may be used in a read/
  /// write barrier.
  bool isOBJCGCCandidate(ASTContext &Ctx) const;

  /// Returns true if this expression is a bound member function.
  bool isBoundMemberFunction(ASTContext &Ctx) const;

  /// Given an expression of bound-member type, find the type
  /// of the member.  Returns null if this is an *overloaded* bound
  /// member expression.
  static QualType findBoundMemberType(const Expr *expr);

  /// Skip past any invisble AST nodes which might surround this
  /// statement, such as ExprWithCleanups or ImplicitCastExpr nodes,
  /// but also injected CXXMemberExpr and CXXConstructExpr which represent
  /// implicit conversions.
  Expr *IgnoreUnlessSpelledInSource();
  const Expr *IgnoreUnlessSpelledInSource() const {
    return const_cast<Expr *>(this)->IgnoreUnlessSpelledInSource();
  }

  /// Skip past any implicit casts which might surround this expression until
  /// reaching a fixed point. Skips:
  /// * ImplicitCastExpr
  /// * FullExpr
  Expr *IgnoreImpCasts() LLVM_READONLY;
  const Expr *IgnoreImpCasts() const {
    return const_cast<Expr *>(this)->IgnoreImpCasts();
  }

  /// Skip past any casts which might surround this expression until reaching
  /// a fixed point. Skips:
  /// * CastExpr
  /// * FullExpr
  /// * MaterializeTemporaryExpr
  /// * SubstNonTypeTemplateParmExpr
  Expr *IgnoreCasts() LLVM_READONLY;
  const Expr *IgnoreCasts() const {
    return const_cast<Expr *>(this)->IgnoreCasts();
  }

  /// Skip past any implicit AST nodes which might surround this expression
  /// until reaching a fixed point. Skips:
  /// * What IgnoreImpCasts() skips
  /// * MaterializeTemporaryExpr
  /// * CXXBindTemporaryExpr
  Expr *IgnoreImplicit() LLVM_READONLY;
  const Expr *IgnoreImplicit() const {
    return const_cast<Expr *>(this)->IgnoreImplicit();
  }

  /// Skip past any implicit AST nodes which might surround this expression
  /// until reaching a fixed point. Same as IgnoreImplicit, except that it
  /// also skips over implicit calls to constructors and conversion functions.
  ///
  /// FIXME: Should IgnoreImplicit do this?
  Expr *IgnoreImplicitAsWritten() LLVM_READONLY;
  const Expr *IgnoreImplicitAsWritten() const {
    return const_cast<Expr *>(this)->IgnoreImplicitAsWritten();
  }

  /// Skip past any parentheses which might surround this expression until
  /// reaching a fixed point. Skips:
  /// * ParenExpr
  /// * UnaryOperator if `UO_Extension`
  /// * GenericSelectionExpr if `!isResultDependent()`
  /// * ChooseExpr if `!isConditionDependent()`
  /// * ConstantExpr
  Expr *IgnoreParens() LLVM_READONLY;
  const Expr *IgnoreParens() const {
    return const_cast<Expr *>(this)->IgnoreParens();
  }

  /// Skip past any parentheses and implicit casts which might surround this
  /// expression until reaching a fixed point.
  /// FIXME: IgnoreParenImpCasts really ought to be equivalent to
  /// IgnoreParens() + IgnoreImpCasts() until reaching a fixed point. However
  /// this is currently not the case. Instead IgnoreParenImpCasts() skips:
  /// * What IgnoreParens() skips
  /// * What IgnoreImpCasts() skips
  /// * MaterializeTemporaryExpr
  /// * SubstNonTypeTemplateParmExpr
  Expr *IgnoreParenImpCasts() LLVM_READONLY;
  const Expr *IgnoreParenImpCasts() const {
    return const_cast<Expr *>(this)->IgnoreParenImpCasts();
  }

  /// Skip past any parentheses and casts which might surround this expression
  /// until reaching a fixed point. Skips:
  /// * What IgnoreParens() skips
  /// * What IgnoreCasts() skips
  Expr *IgnoreParenCasts() LLVM_READONLY;
  const Expr *IgnoreParenCasts() const {
    return const_cast<Expr *>(this)->IgnoreParenCasts();
  }

  /// Skip conversion operators. If this Expr is a call to a conversion
  /// operator, return the argument.
  Expr *IgnoreConversionOperatorSingleStep() LLVM_READONLY;
  const Expr *IgnoreConversionOperatorSingleStep() const {
    return const_cast<Expr *>(this)->IgnoreConversionOperatorSingleStep();
  }

  /// Skip past any parentheses and lvalue casts which might surround this
  /// expression until reaching a fixed point. Skips:
  /// * What IgnoreParens() skips
  /// * What IgnoreCasts() skips, except that only lvalue-to-rvalue
  ///   casts are skipped
  /// FIXME: This is intended purely as a temporary workaround for code
  /// that hasn't yet been rewritten to do the right thing about those
  /// casts, and may disappear along with the last internal use.
  Expr *IgnoreParenLValueCasts() LLVM_READONLY;
  const Expr *IgnoreParenLValueCasts() const {
    return const_cast<Expr *>(this)->IgnoreParenLValueCasts();
  }

  /// Skip past any parenthese and casts which do not change the value
  /// (including ptr->int casts of the same size) until reaching a fixed point.
  /// Skips:
  /// * What IgnoreParens() skips
  /// * CastExpr which do not change the value
  /// * SubstNonTypeTemplateParmExpr
  Expr *IgnoreParenNoopCasts(const ASTContext &Ctx) LLVM_READONLY;
  const Expr *IgnoreParenNoopCasts(const ASTContext &Ctx) const {
    return const_cast<Expr *>(this)->IgnoreParenNoopCasts(Ctx);
  }

  /// Skip past any parentheses and derived-to-base casts until reaching a
  /// fixed point. Skips:
  /// * What IgnoreParens() skips
  /// * CastExpr which represent a derived-to-base cast (CK_DerivedToBase,
  ///   CK_UncheckedDerivedToBase and CK_NoOp)
  Expr *IgnoreParenBaseCasts() LLVM_READONLY;
  const Expr *IgnoreParenBaseCasts() const {
    return const_cast<Expr *>(this)->IgnoreParenBaseCasts();
  }

  /// Determine whether this expression is a default function argument.
  ///
  /// Default arguments are implicitly generated in the abstract syntax tree
  /// by semantic analysis for function calls, object constructions, etc. in
  /// C++. Default arguments are represented by \c CXXDefaultArgExpr nodes;
  /// this routine also looks through any implicit casts to determine whether
  /// the expression is a default argument.
  bool isDefaultArgument() const;

  /// Determine whether the result of this expression is a
  /// temporary object of the given class type.
  bool isTemporaryObject(ASTContext &Ctx, const CXXRecordDecl *TempTy) const;

  /// Whether this expression is an implicit reference to 'this' in C++.
  bool isImplicitCXXThis() const;

  static bool hasAnyTypeDependentArguments(ArrayRef<Expr *> Exprs);

  /// For an expression of class type or pointer to class type,
  /// return the most derived class decl the expression is known to refer to.
  ///
  /// If this expression is a cast, this method looks through it to find the
  /// most derived decl that can be inferred from the expression.
  /// This is valid because derived-to-base conversions have undefined
  /// behavior if the object isn't dynamically of the derived type.
  const CXXRecordDecl *getBestDynamicClassType() const;

  /// Get the inner expression that determines the best dynamic class.
  /// If this is a prvalue, we guarantee that it is of the most-derived type
  /// for the object itself.
  const Expr *getBestDynamicClassTypeExpr() const;

  /// Walk outwards from an expression we want to bind a reference to and
  /// find the expression whose lifetime needs to be extended. Record
  /// the LHSs of comma expressions and adjustments needed along the path.
  const Expr *skipRValueSubobjectAdjustments(
      SmallVectorImpl<const Expr *> &CommaLHS,
      SmallVectorImpl<SubobjectAdjustment> &Adjustments) const;
  const Expr *skipRValueSubobjectAdjustments() const {
    SmallVector<const Expr *, 8> CommaLHSs;
    SmallVector<SubobjectAdjustment, 8> Adjustments;
    return skipRValueSubobjectAdjustments(CommaLHSs, Adjustments);
  }

  /// Checks that the two Expr's will refer to the same value as a comparison
  /// operand.  The caller must ensure that the values referenced by the Expr's
  /// are not modified between E1 and E2 or the result my be invalid.
  static bool isSameComparisonOperand(const Expr* E1, const Expr* E2);

  static bool classof(const Stmt *T) {
    return T->getStmtClass() >= firstExprConstant &&
           T->getStmtClass() <= lastExprConstant;
  }
};
// PointerLikeTypeTraits is specialized so it can be used with a forward-decl of
// Expr. Verify that we got it right.
static_assert(llvm::PointerLikeTypeTraits<Expr *>::NumLowBitsAvailable <=
                  llvm::detail::ConstantLog2<alignof(Expr)>::value,
              "PointerLikeTypeTraits<Expr*> assumes too much alignment.");

using ConstantExprKind = Expr::ConstantExprKind;

//===----------------------------------------------------------------------===//
// Wrapper Expressions.
//===----------------------------------------------------------------------===//

/// FullExpr - Represents a "full-expression" node.
class FullExpr : public Expr {
protected:
 Stmt *SubExpr;

 FullExpr(StmtClass SC, Expr *subexpr)
     : Expr(SC, subexpr->getType(), subexpr->getValueKind(),
            subexpr->getObjectKind()),
       SubExpr(subexpr) {
 }
  FullExpr(StmtClass SC, EmptyShell Empty)
    : Expr(SC, Empty) {}
public:
  const Expr *getSubExpr() const { return cast<Expr>(SubExpr); }
  Expr *getSubExpr() { return cast<Expr>(SubExpr); }

  /// As with any mutator of the AST, be very careful when modifying an
  /// existing AST to preserve its invariants.
  void setSubExpr(Expr *E) { SubExpr = E; }

  static bool classof(const Stmt *T) {
    return T->getStmtClass() >= firstFullExprConstant &&
           T->getStmtClass() <= lastFullExprConstant;
  }
};

/// Represents an expression -- generally a full-expression -- that
/// introduces cleanups to be run at the end of the sub-expression's
/// evaluation.  The most common source of expression-introduced
/// cleanups is temporary objects in C++, but several other kinds of
/// expressions can create cleanups, including basically every
/// call in ARC that returns an Objective-C pointer.
///
/// This expression also tracks whether the sub-expression contains a
/// potentially-evaluated block literal.  The lifetime of a block
/// literal is the extent of the enclosing scope.
class ExprWithCleanups final
    : public FullExpr,
      private llvm::TrailingObjects<
          ExprWithCleanups,
          llvm::PointerUnion<BlockDecl *, CompoundLiteralExpr *>> {
public:
  /// The type of objects that are kept in the cleanup.
  /// It's useful to remember the set of blocks and block-scoped compound
  /// literals; we could also remember the set of temporaries, but there's
  /// currently no need.
  using CleanupObject = llvm::PointerUnion<BlockDecl *, CompoundLiteralExpr *>;

private:
  friend class ASTStmtReader;
  friend TrailingObjects;

  ExprWithCleanups(EmptyShell, unsigned NumObjects);
  ExprWithCleanups(Expr *SubExpr, bool CleanupsHaveSideEffects,
                   ArrayRef<CleanupObject> Objects);

public:
  static ExprWithCleanups *Create(const ASTContext &C, EmptyShell empty,
                                  unsigned numObjects);

  static ExprWithCleanups *Create(const ASTContext &C, Expr *subexpr,
                                  bool CleanupsHaveSideEffects,
                                  ArrayRef<CleanupObject> objects);

  ArrayRef<CleanupObject> getObjects() const {
    return llvm::makeArrayRef(getTrailingObjects<CleanupObject>(),
                              getNumObjects());
  }

  unsigned getNumObjects() const;

  CleanupObject getObject(unsigned i) const {
    assert(i < getNumObjects() && "Index out of range");
    return getObjects()[i];
  }

  bool cleanupsHaveSideEffects() const;

  SourceLocation getBeginLoc() const LLVM_READONLY {
    return SubExpr->getBeginLoc();
  }

  SourceLocation getEndLoc() const LLVM_READONLY {
    return SubExpr->getEndLoc();
  }

  // Implement isa/cast/dyncast/etc.
  static bool classof(const Stmt *T) {
    return T->getStmtClass() == ExprWithCleanupsClass;
  }

  // Iterators
  child_range children();

  const_child_range children() const;
};

}
} // end namespace clang

#endif // _H
