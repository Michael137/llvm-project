//===------------------ ItaniumDemangleTest.cpp ---------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Demangle/ItaniumDemangle.h"
#include "ItaniumDemangleTestUtils.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/Support/Allocator.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <vector>

using namespace llvm;
using namespace llvm::itanium_demangle;
using namespace llvm::itanium_demangle::test_utils;

namespace NodeMatcher {
// Make sure the node matchers provide constructor parameters. This is a
// compilation test.
template <typename NT> struct Ctor {
  template <typename... Args> void operator()(Args &&...args) {
    auto _ = NT(std::forward<Args>(args)...);
  }
};

template <typename NT> void Visit(const NT *Node) { Node->match(Ctor<NT>{}); }
#define NOMATCHER(X)                                                           \
  template <> void Visit<itanium_demangle::X>(const itanium_demangle::X *) {}
// Some nodes have no match member.
NOMATCHER(ForwardTemplateReference)
#undef NOMATCHER

void Visitor() {
#define NODE(X) Visit(static_cast<const itanium_demangle::X *>(nullptr));
#include "llvm/Demangle/ItaniumNodes.def"
}
} // namespace NodeMatcher

// Verify Operator table is ordered
TEST(ItaniumDemangle, OperatorOrdering) {
  struct TestParser : AbstractManglingParser<TestParser, TestAllocator> {};
  for (const auto *Op = &TestParser::Ops[0];
       Op != &TestParser::Ops[TestParser::NumOps - 1]; Op++)
    ASSERT_LT(Op[0], Op[1]);
}

TEST(ItaniumDemangle, MethodOverride) {
  struct TestParser : AbstractManglingParser<TestParser, TestAllocator> {
    std::vector<char> Types;

    TestParser(const char *Str)
        : AbstractManglingParser(Str, Str + strlen(Str)) {}

    Node *parseType() {
      Types.push_back(*First);
      return AbstractManglingParser<TestParser, TestAllocator>::parseType();
    }
  };

  TestParser Parser("_Z1fIiEjl");
  ASSERT_NE(nullptr, Parser.parse());
  EXPECT_THAT(Parser.Types, testing::ElementsAre('i', 'j', 'l'));
}

static std::string toString(OutputBuffer &OB) {
  StringView SV = OB;
  return {SV.begin(), SV.end()};
}

TEST(ItaniumDemangle, HalfType) {
  struct TestParser : AbstractManglingParser<TestParser, TestAllocator> {
    std::vector<std::string> Types;

    TestParser(const char *Str)
        : AbstractManglingParser(Str, Str + strlen(Str)) {}

    Node *parseType() {
      OutputBuffer OB;
      Node *N = AbstractManglingParser<TestParser, TestAllocator>::parseType();
      N->printLeft(OB);
      StringView Name = N->getBaseName();
      if (!Name.empty())
        Types.push_back(std::string(Name.begin(), Name.end()));
      else
        Types.push_back(toString(OB));
      std::free(OB.getBuffer());
      return N;
    }
  };

  // void f(A<_Float16>, _Float16);
  TestParser Parser("_Z1f1AIDF16_EDF16_");
  ASSERT_NE(nullptr, Parser.parse());
  EXPECT_THAT(Parser.Types, testing::ElementsAre("_Float16", "A", "_Float16"));
}

TEST(ItaniumDemangle, Equality_NodeArray) {
  ManglingNodeCreator Creator;

  // Test empty NodeArray equality
  std::vector<Node *> LHS;
  std::vector<Node *> RHS;

  // Empty == Empty
  ASSERT_EQ(Creator.makeNodeArray(LHS.cbegin(), LHS.cend()),
            Creator.makeNodeArray(RHS.cbegin(), RHS.cend()));

  RHS.push_back(Creator.makeReferenceType(Qualifiers::QualNone, "test",
                                          ReferenceKind::RValue));

  // Empty vs. non-empty
  ASSERT_NE(Creator.makeNodeArray(LHS.cbegin(), LHS.cend()),
            Creator.makeNodeArray(RHS.cbegin(), RHS.cend()));

  LHS.push_back(Creator.makeReferenceType(Qualifiers::QualNone, "test",
                                          ReferenceKind::RValue));

  // Equal single nodes
  ASSERT_EQ(Creator.makeNodeArray(LHS.cbegin(), LHS.cend()),
            Creator.makeNodeArray(RHS.cbegin(), RHS.cend()));

  LHS.push_back(Creator.makeReferenceType(Qualifiers::QualNone, "test",
                                          ReferenceKind::LValue));
  RHS.push_back(Creator.makeReferenceType(Qualifiers::QualRestrict, "test",
                                          ReferenceKind::RValue));

  // Non-equal second node
  ASSERT_NE(Creator.makeNodeArray(LHS.cbegin(), LHS.cend()),
            Creator.makeNodeArray(RHS.cbegin(), RHS.cend()));
}

TEST(ItaniumDemangle, Equality_Node) {
  ManglingNodeCreator Creator;

  // Comparison of nodes with mismatching kinds should fail
  ASSERT_FALSE(test_utils::compareNodes(Creator.make<NameType>("Foo"),
                                        Creator.make<FloatLiteral>("1.0")));

  {
    // Mismatching precendences
    Node const *LHS = Creator.make<CastExpr>("dynamic_cast",
                                             Creator.make<NameType>("Foo"),
                                             Creator.make<NameType>("Bar"),
                                             Node::Prec::Equality);

    Node const *RHS = Creator.make<CastExpr>("dynamic_cast",
                                             Creator.make<NameType>("Foo"),
                                             Creator.make<NameType>("Bar"),
                                             Node::Prec::Cast);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_QualType) {
  ManglingNodeCreator Creator;

  {
    // Same qualifiers and name
    Node const *Q1 = Creator.makeQualType("Test", Qualifiers::QualVolatile);
    Node const *Q2 = Creator.makeQualType("Test", Qualifiers::QualVolatile);
    ASSERT_TRUE(test_utils::compareNodes(Q1, Q2));
  }

  {
    // Different qualifiers
    Node const *Q1 = Creator.makeQualType("Test", Qualifiers::QualNone);
    Node const *Q2 = Creator.makeQualType("Test", Qualifiers::QualRestrict);
    ASSERT_FALSE(test_utils::compareNodes(Q1, Q2));
  }

  {
    // Different name
    Node const *Q1 = Creator.makeQualType("Test", Qualifiers::QualNone);
    Node const *Q2 = Creator.makeQualType("", Qualifiers::QualNone);
    ASSERT_FALSE(test_utils::compareNodes(Q1, Q2));
  }
}

TEST(ItaniumDemangle, Equality_NameType) {
  ManglingNodeCreator Creator;

  {
    // Both empty
    Node const *Q1 = Creator.make<NameType>("");
    Node const *Q2 = Creator.make<NameType>("");
    ASSERT_TRUE(test_utils::compareNodes(Q1, Q2));
  }

  {
    // Both non-empty
    Node const *Q1 = Creator.make<NameType>("test");
    Node const *Q2 = Creator.make<NameType>("test");
    ASSERT_TRUE(test_utils::compareNodes(Q1, Q2));
  }

  {
    // Not equal
    Node const *Q1 = Creator.make<NameType>("test");
    Node const *Q2 = Creator.make<NameType>("tes");
    ASSERT_FALSE(test_utils::compareNodes(Q1, Q2));
  }
}

TEST(ItaniumDemangle, Equality_ReferenceType) {
  ManglingNodeCreator Creator;

  {
    // Same type
    Node const *Q1 = Creator.makeReferenceType(Qualifiers::QualVolatile, "Test",
                                               ReferenceKind::LValue);
    Node const *Q2 = Creator.makeReferenceType(Qualifiers::QualVolatile, "Test",
                                               ReferenceKind::LValue);
    ASSERT_TRUE(test_utils::compareNodes(Q1, Q2));
  }

  {
    // Different name
    Node const *Q1 = Creator.makeReferenceType(Qualifiers::QualVolatile, "Test",
                                               ReferenceKind::LValue);
    Node const *Q2 = Creator.makeReferenceType(Qualifiers::QualVolatile, "Tes",
                                               ReferenceKind::LValue);
    ASSERT_FALSE(test_utils::compareNodes(Q1, Q2));
  }

  {
    // Different CV-qualifiers
    Node const *Q1 = Creator.makeReferenceType(Qualifiers::QualVolatile, "Test",
                                               ReferenceKind::LValue);
    Node const *Q2 = Creator.makeReferenceType(Qualifiers::QualRestrict, "Test",
                                               ReferenceKind::LValue);
    ASSERT_FALSE(test_utils::compareNodes(Q1, Q2));
  }

  {
    // Different ref-qualifiers
    Node const *Q1 = Creator.makeReferenceType(Qualifiers::QualVolatile, "Test",
                                               ReferenceKind::LValue);
    Node const *Q2 = Creator.makeReferenceType(Qualifiers::QualVolatile, "Test",
                                               ReferenceKind::RValue);
    ASSERT_FALSE(test_utils::compareNodes(Q1, Q2));
  }
}

TEST(ItaniumDemangle, Equality_TemplateArgs) {
  ManglingNodeCreator Creator;

  std::vector<Node *> LHS;
  std::vector<Node *> RHS;

  {
    // Both empty
    Node const *Q1 = Creator.makeTemplateArgs(LHS);
    Node const *Q2 = Creator.makeTemplateArgs(RHS);
    ASSERT_TRUE(test_utils::compareNodes(Q1, Q2));
  }

  LHS.push_back(Creator.make<NameType>("test"));
  RHS.push_back(Creator.make<NameType>("test"));

  {
    // Both equal
    Node const *Q1 = Creator.makeTemplateArgs(LHS);
    Node const *Q2 = Creator.makeTemplateArgs(RHS);
    ASSERT_TRUE(test_utils::compareNodes(Q1, Q2));
  }

  LHS.push_back(Creator.make<NameType>("foo"));
  RHS.push_back(Creator.make<NameType>("bar"));

  {
    // Not equal anymore
    Node const *Q1 = Creator.makeTemplateArgs(LHS);
    Node const *Q2 = Creator.makeTemplateArgs(RHS);
    ASSERT_FALSE(test_utils::compareNodes(Q1, Q2));
  }
}

TEST(ItaniumDemangle, Equality_NameWithTemplateArgs) {
  ManglingNodeCreator Creator;

  {
    // No args
    Node const *Q1 = Creator.makeNameWithTemplateArgs("Foo", {});
    Node const *Q2 = Creator.makeNameWithTemplateArgs("Foo", {});
    ASSERT_TRUE(test_utils::compareNodes(Q1, Q2));
  }

  {
    // Equal
    Node const *Q1 = Creator.makeNameWithTemplateArgs(
        "Foo", {Creator.make<NameType>("foo")});
    Node const *Q2 = Creator.makeNameWithTemplateArgs(
        "Foo", {Creator.make<NameType>("foo")});
    ASSERT_TRUE(test_utils::compareNodes(Q1, Q2));
  }

  {
    // Different name
    Node const *Q1 = Creator.makeNameWithTemplateArgs(
        "Foo", {Creator.make<NameType>("foo")});
    Node const *Q2 = Creator.makeNameWithTemplateArgs(
        "Bar", {Creator.make<NameType>("foo")});
    ASSERT_FALSE(test_utils::compareNodes(Q1, Q2));
  }

  {
    // Different args
    Node const *Q1 = Creator.makeNameWithTemplateArgs(
        "Foo", {Creator.make<NameType>("foo"), Creator.make<NameType>("bar")});

    Node const *Q2 = Creator.makeNameWithTemplateArgs(
        "Foo", {Creator.make<NameType>("bar"), Creator.make<NameType>("foo")});
    ASSERT_FALSE(test_utils::compareNodes(Q1, Q2));
  }
}

TEST(ItaniumDemangle, Equality_NodeArrayNode) {
  ManglingNodeCreator Creator;

  std::vector<Node *> LHS;
  std::vector<Node *> RHS;

  {
    Node const *Q1 = Creator.make<NodeArrayNode>(
        Creator.makeNodeArray(LHS.cbegin(), LHS.cend()));
    Node const *Q2 = Creator.make<NodeArrayNode>(
        Creator.makeNodeArray(RHS.cbegin(), RHS.cend()));
    ASSERT_TRUE(test_utils::compareNodes(Q1, Q2));
  }

  LHS.push_back(Creator.make<NameType>("Foo"));
  RHS.push_back(Creator.make<NameType>("Foo"));

  {
    // Equal
    Node const *Q1 = Creator.make<NodeArrayNode>(
        Creator.makeNodeArray(LHS.cbegin(), LHS.cend()));
    Node const *Q2 = Creator.make<NodeArrayNode>(
        Creator.makeNodeArray(RHS.cbegin(), RHS.cend()));
    ASSERT_TRUE(test_utils::compareNodes(Q1, Q2));
  }

  LHS.push_back(Creator.make<NameType>("Bar"));
  RHS.push_back(Creator.make<NameType>("Qux"));

  {
    // Not equal
    Node const *Q1 = Creator.make<NodeArrayNode>(
        Creator.makeNodeArray(LHS.cbegin(), LHS.cend()));
    Node const *Q2 = Creator.make<NodeArrayNode>(
        Creator.makeNodeArray(RHS.cbegin(), RHS.cend()));
    ASSERT_FALSE(test_utils::compareNodes(Q1, Q2));
  }
}

TEST(ItaniumDemangle, Equality_DotSuffix) {
  ManglingNodeCreator Creator;

  std::vector<Node *> ParamNodes{
      Creator.makeReferenceType(Qualifiers::QualVolatile, "Foo",
                                ReferenceKind::LValue),
      Creator.makeReferenceType(Qualifiers::QualRestrict, "Bar",
                                ReferenceKind::RValue)};

  Node const *Func1 = Creator.makeFunctionEncoding("Baz", ParamNodes, {}, {});
  Node const *Func2 = Creator.makeFunctionEncoding("Qux", ParamNodes, {}, {});
  ASSERT_FALSE(test_utils::compareNodes(Func1, Func2));

  {
    // Equal
    Node const *LHS = Creator.make<DotSuffix>(Func1, "foo");
    Node const *RHS = Creator.make<DotSuffix>(Func1, "foo");
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different suffix
    Node const *LHS = Creator.make<DotSuffix>(Func1, "foo");
    Node const *RHS = Creator.make<DotSuffix>(Func1, "bar");
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different prefix
    Node const *LHS = Creator.make<DotSuffix>(Func1, "foo");
    Node const *RHS = Creator.make<DotSuffix>(Func2, "foo");
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_NestedName) {
  ManglingNodeCreator Creator;

  auto *Scope1 = Creator.make<NameType>("S1");
  auto *Scope2 = Creator.make<NameType>("S2");

  auto *Name1 = Creator.make<NameType>("Foo");
  auto *Name2 = Creator.make<NameType>("Bar");

  {
    // Equal
    Node const *LHS = Creator.make<NestedName>(Scope1, Name1);
    Node const *RHS = Creator.make<NestedName>(Scope1, Name1);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different scope
    Node const *LHS = Creator.make<NestedName>(Scope1, Name1);
    Node const *RHS = Creator.make<NestedName>(Scope2, Name1);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different name
    Node const *LHS = Creator.make<NestedName>(Scope1, Name1);
    Node const *RHS = Creator.make<NestedName>(Scope1, Name2);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_ModuleName) {
  ManglingNodeCreator Creator;

  auto *Name1 = Creator.make<NameType>("Foo");
  auto *Name2 = Creator.make<NameType>("Bar");

  ModuleName *Parent1 =
      static_cast<ModuleName *>(Creator.make<ModuleName>(nullptr, Name1, true));

  ModuleName *Parent2 = static_cast<ModuleName *>(
      Creator.make<ModuleName>(Parent1, Name2, false));

  {
    // Equal
    Node const *LHS = Creator.make<ModuleName>(Parent1, Name1, true);
    Node const *RHS = Creator.make<ModuleName>(Parent1, Name1, true);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different parents
    Node const *LHS = Creator.make<ModuleName>(Parent1, Name1, true);
    Node const *RHS = Creator.make<ModuleName>(Parent2, Name1, true);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different name
    Node const *LHS = Creator.make<ModuleName>(Parent2, Name1, true);
    Node const *RHS = Creator.make<ModuleName>(Parent2, Name2, true);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different partition type
    Node const *LHS = Creator.make<ModuleName>(Parent2, Name1, true);
    Node const *RHS = Creator.make<ModuleName>(Parent2, Name1, false);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_LocalName) {
  ManglingNodeCreator Creator;

  std::vector<Node *> ParamNodes{
      Creator.makeReferenceType(Qualifiers::QualVolatile, "Foo",
                                ReferenceKind::LValue),
      Creator.makeReferenceType(Qualifiers::QualRestrict, "Bar",
                                ReferenceKind::RValue)};

  Node *Func1 = Creator.makeFunctionEncoding("Baz", ParamNodes, {}, {});
  Node *Func2 = Creator.makeFunctionEncoding("Qux", ParamNodes, {}, {});
  ASSERT_FALSE(test_utils::compareNodes(Func1, Func2));

  Node *Name1 = Creator.make<NestedName>(Creator.make<NameType>("Scope1"),
                                         Creator.make<NameType>("Name1"));

  Node *Name2 = Creator.make<NestedName>(Creator.make<NameType>("Scope2"),
                                         Creator.make<NameType>("Name2"));

  {
    // Equal
    Node const *LHS = Creator.make<LocalName>(Func1, Name1);
    Node const *RHS = Creator.make<LocalName>(Func1, Name1);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different encoding
    Node const *LHS = Creator.make<LocalName>(Func1, Name1);
    Node const *RHS = Creator.make<LocalName>(Func2, Name1);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different name
    Node const *LHS = Creator.make<LocalName>(Func1, Name2);
    Node const *RHS = Creator.make<LocalName>(Func1, Name1);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_QualifiedName) {
  ManglingNodeCreator Creator;

  auto *Scope1 = Creator.make<NameType>("Foo");
  auto *Scope2 = Creator.make<NameType>("Bar");

  auto *Name1 = Creator.make<NameType>("foo");
  auto *Name2 = Creator.make<NameType>("bar");

  {
    // Equal
    Node const *LHS = Creator.make<QualifiedName>(Scope1, Name1);
    Node const *RHS = Creator.make<QualifiedName>(Scope1, Name1);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different scope
    Node const *LHS = Creator.make<QualifiedName>(Scope1, Name1);
    Node const *RHS = Creator.make<QualifiedName>(Scope2, Name1);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different name
    Node const *LHS = Creator.make<QualifiedName>(Scope1, Name2);
    Node const *RHS = Creator.make<QualifiedName>(Scope1, Name1);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_CtorDtorName) {
  ManglingNodeCreator Creator;

  auto *Name1 = Creator.make<NameType>("Foo");
  auto *Name2 = Creator.make<NameType>("Bar");

  int Variant1 = '5' - '0';
  int Variant2 = '4' - '0';

  {
    // Equal
    Node const *LHS = Creator.make<CtorDtorName>(Name1, true, Variant1);
    Node const *RHS = Creator.make<CtorDtorName>(Name1, true, Variant1);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different name
    Node const *LHS = Creator.make<CtorDtorName>(Name1, true, Variant1);
    Node const *RHS = Creator.make<CtorDtorName>(Name2, true, Variant1);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Constructor vs. Destructor
    Node const *LHS = Creator.make<CtorDtorName>(Name1, true, Variant1);
    Node const *RHS = Creator.make<CtorDtorName>(Name1, false, Variant1);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different variant
    Node const *LHS = Creator.make<CtorDtorName>(Name1, false, Variant2);
    Node const *RHS = Creator.make<CtorDtorName>(Name1, false, Variant1);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_DtorName) {
  ManglingNodeCreator Creator;

  auto *Name1 = Creator.make<NameType>("Foo");
  auto *Name2 = Creator.make<NameType>("Bar");

  {
    // Equal
    Node const *LHS = Creator.make<DtorName>(Name1);
    Node const *RHS = Creator.make<DtorName>(Name1);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different name
    Node const *LHS = Creator.make<DtorName>(Name1);
    Node const *RHS = Creator.make<DtorName>(Name2);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_UnnamedTypename) {
  ManglingNodeCreator Creator;

  {
    // Equal
    Node const *LHS = Creator.make<UnnamedTypeName>("Foo");
    Node const *RHS = Creator.make<UnnamedTypeName>("Foo");
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different name
    Node const *LHS = Creator.make<UnnamedTypeName>("Foo");
    Node const *RHS = Creator.make<UnnamedTypeName>("Bar");
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_ClosureTypeName) {
  ManglingNodeCreator Creator;

  std::vector<Node *> Params1{Creator.make<NameType>("double"),
                              Creator.make<NameType>("int")};

  std::vector<Node *> Params2{Creator.make<NameType>("double"),
                              Creator.make<NameType>("std::string")};

  std::vector<Node *> TemplateParams1{Creator.make<NameType>("Foo"),
                                      Creator.make<NameType>("Bar")};

  std::vector<Node *> TemplateParams2{Creator.make<NameType>("Bar"),
                                      Creator.make<NameType>("Foo")};

  {
    // Equal
    Node const *LHS = Creator.make<ClosureTypeName>(
        Creator.makeNodeArray(Params1.cbegin(), Params1.cend()),
        Creator.makeNodeArray(TemplateParams1.cbegin(), TemplateParams1.cend()),
        "Foo");

    Node const *RHS = Creator.make<ClosureTypeName>(
        Creator.makeNodeArray(Params1.cbegin(), Params1.cend()),
        Creator.makeNodeArray(TemplateParams1.cbegin(), TemplateParams1.cend()),
        "Foo");

    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different name
    Node const *LHS = Creator.make<ClosureTypeName>(
        Creator.makeNodeArray(Params1.cbegin(), Params1.cend()),
        Creator.makeNodeArray(TemplateParams1.cbegin(), TemplateParams1.cend()),
        "Foo");

    Node const *RHS = Creator.make<ClosureTypeName>(
        Creator.makeNodeArray(Params1.cbegin(), Params1.cend()),
        Creator.makeNodeArray(TemplateParams1.cbegin(), TemplateParams1.cend()),
        "Bar");

    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different params
    Node const *LHS = Creator.make<ClosureTypeName>(
        Creator.makeNodeArray(Params1.cbegin(), Params1.cend()),
        Creator.makeNodeArray(TemplateParams1.cbegin(), TemplateParams1.cend()),
        "Foo");

    Node const *RHS = Creator.make<ClosureTypeName>(
        Creator.makeNodeArray(Params2.cbegin(), Params2.cend()),
        Creator.makeNodeArray(TemplateParams1.cbegin(), TemplateParams1.cend()),
        "Foo");

    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different template params
    Node const *LHS = Creator.make<ClosureTypeName>(
        Creator.makeNodeArray(Params1.cbegin(), Params1.cend()),
        Creator.makeNodeArray(TemplateParams1.cbegin(), TemplateParams1.cend()),
        "Foo");

    Node const *RHS = Creator.make<ClosureTypeName>(
        Creator.makeNodeArray(Params1.cbegin(), Params1.cend()),
        Creator.makeNodeArray(TemplateParams2.cbegin(), TemplateParams2.cend()),
        "Foo");

    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_StructuredBindingName) {
  ManglingNodeCreator Creator;

  std::vector<Node *> Params1{Creator.make<NameType>("Foo"),
                              Creator.make<NameType>("Bar")};

  std::vector<Node *> Params2{Creator.make<NameType>("Bar"),
                              Creator.make<NameType>("Foo")};

  {
    // Equal
    Node const *LHS = Creator.make<StructuredBindingName>(
        Creator.makeNodeArray(Params1.cbegin(), Params1.cend()));
    Node const *RHS = Creator.make<StructuredBindingName>(
        Creator.makeNodeArray(Params1.cbegin(), Params1.cend()));

    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different bindings
    Node const *LHS = Creator.make<StructuredBindingName>(
        Creator.makeNodeArray(Params1.cbegin(), Params1.cend()));
    Node const *RHS = Creator.make<StructuredBindingName>(
        Creator.makeNodeArray(Params2.cbegin(), Params2.cend()));

    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_LiteralOperator) {
  ManglingNodeCreator Creator;

  auto *Name1 = Creator.make<NameType>("==");
  auto *Name2 = Creator.make<NameType>("+");

  {
    // Equal
    Node const *LHS = Creator.make<LiteralOperator>(Name1);
    Node const *RHS = Creator.make<LiteralOperator>(Name1);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different name
    Node const *LHS = Creator.make<LiteralOperator>(Name1);
    Node const *RHS = Creator.make<LiteralOperator>(Name2);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_SpecialName) {
  ManglingNodeCreator Creator;

  auto *Name1 = Creator.make<NameType>("Foo");
  auto *Name2 = Creator.make<NameType>("Bar");

  {
    // Equal
    Node const *LHS = Creator.make<SpecialName>("vtable for ", Name1);
    Node const *RHS = Creator.make<SpecialName>("vtable for ", Name1);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different prefix
    Node const *LHS = Creator.make<SpecialName>("VTT for ", Name1);
    Node const *RHS = Creator.make<SpecialName>("vtable for ", Name1);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different child
    Node const *LHS = Creator.make<SpecialName>("vtable for ", Name1);
    Node const *RHS = Creator.make<SpecialName>("vtable for ", Name2);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_CtorVtableSpecialName) {
  ManglingNodeCreator Creator;

  auto *FstType1 = Creator.make<NameType>("Foo");
  auto *FstType2 = Creator.make<NameType>("Bar");

  auto *SndType1 = Creator.make<NameType>("Baz");
  auto *SndType2 = Creator.make<NameType>("Quz");

  {
    // Equal
    Node const *LHS = Creator.make<CtorVtableSpecialName>(FstType1, SndType1);
    Node const *RHS = Creator.make<CtorVtableSpecialName>(FstType1, SndType1);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different first type
    Node const *LHS = Creator.make<CtorVtableSpecialName>(FstType1, SndType1);
    Node const *RHS = Creator.make<CtorVtableSpecialName>(FstType2, SndType1);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different second type
    Node const *LHS = Creator.make<CtorVtableSpecialName>(FstType1, SndType1);
    Node const *RHS = Creator.make<CtorVtableSpecialName>(FstType1, SndType2);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_StringLiteral) {
  ManglingNodeCreator Creator;

  auto *Type1 = Creator.make<NameType>("std::string");
  auto *Type2 = Creator.make<NameType>("char const*");

  {
    // Equal
    Node const *LHS = Creator.make<itanium_demangle::StringLiteral>(Type1);
    Node const *RHS = Creator.make<itanium_demangle::StringLiteral>(Type1);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different type
    Node const *LHS = Creator.make<itanium_demangle::StringLiteral>(Type1);
    Node const *RHS = Creator.make<itanium_demangle::StringLiteral>(Type2);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_EnumLiteral) {
  ManglingNodeCreator Creator;

  {
    // Equal
    Node const *LHS = Creator.make<IntegerLiteral>("Enum1", "100");
    Node const *RHS = Creator.make<IntegerLiteral>("Enum1", "100");
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different type
    Node const *LHS = Creator.make<IntegerLiteral>("Enum1", "100");
    Node const *RHS = Creator.make<IntegerLiteral>("Enum2", "100");
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different value
    Node const *LHS = Creator.make<IntegerLiteral>("Enum1", "100");
    Node const *RHS = Creator.make<IntegerLiteral>("Enum1", "50");
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_FloatLiteral) {
  ManglingNodeCreator Creator;

  {
    // Equal
    Node const *LHS = Creator.make<FloatLiteral>("1.0");
    Node const *RHS = Creator.make<FloatLiteral>("1.0");
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different value
    Node const *LHS = Creator.make<FloatLiteral>("1.0");
    Node const *RHS = Creator.make<FloatLiteral>("2.0");
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_LambdaExpr) {
  ManglingNodeCreator Creator;
  std::vector<Node *> Params{Creator.make<NameType>("double"),
                             Creator.make<NameType>("int")};

  std::vector<Node *> TemplateParams{Creator.make<NameType>("Foo"),
                                     Creator.make<NameType>("Bar")};

  // Equal
  Node const *Closure1 = Creator.make<ClosureTypeName>(
      Creator.makeNodeArray(Params.cbegin(), Params.cend()),
      Creator.makeNodeArray(TemplateParams.cbegin(), TemplateParams.cend()),
      "Closure1");

  Node const *Closure2 = Creator.make<ClosureTypeName>(
      Creator.makeNodeArray(Params.cbegin(), Params.cend()),
      Creator.makeNodeArray(TemplateParams.cbegin(), TemplateParams.cend()),
      "Closure2");

  {
    // Equal
    Node const *LHS = Creator.make<LambdaExpr>(Closure1);
    Node const *RHS = Creator.make<LambdaExpr>(Closure1);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different closure types
    Node const *LHS = Creator.make<LambdaExpr>(Closure1);
    Node const *RHS = Creator.make<LambdaExpr>(Closure2);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_VendorExtQualType) {
  ManglingNodeCreator Creator;
  auto *Name1 = Creator.make<NameType>("foo");
  auto *Name2 = Creator.make<NameType>("bar");

  std::vector<Node *> Params1{Creator.make<NameType>("double"),
                              Creator.make<NameType>("int")};

  std::vector<Node *> Params2{Creator.make<NameType>("int")};

  auto const *Args1 = Creator.make<TemplateArgs>(
      Creator.makeNodeArray(Params1.cbegin(), Params1.cend()));

  auto const *Args2 = Creator.make<TemplateArgs>(
      Creator.makeNodeArray(Params2.cbegin(), Params2.cend()));

  {
    // Equal
    Node const *LHS = Creator.make<VendorExtQualType>(Name1, "Ext", nullptr);
    Node const *RHS = Creator.make<VendorExtQualType>(Name1, "Ext", nullptr);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different qualified name
    Node const *LHS = Creator.make<VendorExtQualType>(Name1, "Ext", nullptr);
    Node const *RHS = Creator.make<VendorExtQualType>(Name2, "Ext", nullptr);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different ext name
    Node const *LHS = Creator.make<VendorExtQualType>(Name1, "Ext", nullptr);
    Node const *RHS = Creator.make<VendorExtQualType>(Name1, "", nullptr);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different template args
    Node const *LHS = Creator.make<VendorExtQualType>(Name1, "Ext", Args1);
    Node const *RHS = Creator.make<VendorExtQualType>(Name1, "Ext", nullptr);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different template args
    Node const *LHS = Creator.make<VendorExtQualType>(Name1, "Ext", nullptr);
    Node const *RHS = Creator.make<VendorExtQualType>(Name1, "Ext", Args1);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different template args
    Node const *LHS = Creator.make<VendorExtQualType>(Name1, "Ext", Args1);
    Node const *RHS = Creator.make<VendorExtQualType>(Name1, "Ext", Args2);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_ConversionOperatorType) {
  ManglingNodeCreator Creator;

  auto *Name1 = Creator.make<NameType>("int");
  auto *Name2 = Creator.make<NameType>("double");

  {
    // Equal
    Node const *LHS = Creator.make<PostfixQualifiedType>(Name1, "complex");
    Node const *RHS = Creator.make<PostfixQualifiedType>(Name1, "complex");
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different typenames
    Node const *LHS = Creator.make<PostfixQualifiedType>(Name1, "complex");
    Node const *RHS = Creator.make<PostfixQualifiedType>(Name2, "complex");
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different postfix
    Node const *LHS = Creator.make<PostfixQualifiedType>(Name1, "complex");
    Node const *RHS = Creator.make<PostfixQualifiedType>(Name1, "imaginary");
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_BitIntType) {
  ManglingNodeCreator Creator;

  auto *Size1 = Creator.make<NameType>("32");
  auto *Size2 = Creator.make<NameType>("64");

  {
    // Equal
    Node const *LHS = Creator.make<BitIntType>(Size1, true);
    Node const *RHS = Creator.make<BitIntType>(Size1, true);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different sizes
    Node const *LHS = Creator.make<BitIntType>(Size1, true);
    Node const *RHS = Creator.make<BitIntType>(Size2, true);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different signdness
    Node const *LHS = Creator.make<BitIntType>(Size1, true);
    Node const *RHS = Creator.make<BitIntType>(Size1, false);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_ElaboratedTypeSpefType) {
  ManglingNodeCreator Creator;

  auto *Name1 = Creator.make<NameType>("Foo");
  auto *Name2 = Creator.make<NameType>("Bar");

  {
    // Equal
    Node const *LHS = Creator.make<ElaboratedTypeSpefType>("enum", Name1);
    Node const *RHS = Creator.make<ElaboratedTypeSpefType>("enum", Name1);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different names
    Node const *LHS = Creator.make<ElaboratedTypeSpefType>("enum", Name1);
    Node const *RHS = Creator.make<ElaboratedTypeSpefType>("enum", Name2);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different typenames
    Node const *LHS = Creator.make<ElaboratedTypeSpefType>("enum", Name1);
    Node const *RHS = Creator.make<ElaboratedTypeSpefType>("union", Name1);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_AbiTagAttr) {
  ManglingNodeCreator Creator;

  std::vector<Node *> ParamNodes{
      Creator.makeReferenceType(Qualifiers::QualVolatile, "Foo",
                                ReferenceKind::LValue),
      Creator.makeReferenceType(Qualifiers::QualRestrict, "Bar",
                                ReferenceKind::RValue)};

  Node *Func1 = Creator.makeFunctionEncoding("Baz", ParamNodes, {}, {});
  Node *Func2 = Creator.makeFunctionEncoding("Qux", ParamNodes, {}, {});
  ASSERT_FALSE(test_utils::compareNodes(Func1, Func2));

  {
    // Equal
    Node const *LHS = Creator.make<AbiTagAttr>(Func1, "v1");
    Node const *RHS = Creator.make<AbiTagAttr>(Func1, "v1");
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different function prototype
    Node const *LHS = Creator.make<AbiTagAttr>(Func1, "v1");
    Node const *RHS = Creator.make<AbiTagAttr>(Func2, "v1");
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different tag
    Node const *LHS = Creator.make<AbiTagAttr>(Func1, "v1");
    Node const *RHS = Creator.make<AbiTagAttr>(Func1, "v2");
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_EnableIfAttr) {
  ManglingNodeCreator Creator;

  std::vector<Node *> Params1{Creator.make<NameType>("double"),
                              Creator.make<NameType>("int")};

  std::vector<Node *> Params2{Creator.make<NameType>("int")};

  NodeArray Args1 = Creator.makeNodeArray(Params1.cbegin(), Params1.cend());
  NodeArray Args2 = Creator.makeNodeArray(Params2.cbegin(), Params2.cend());

  {
    // Equal
    Node const *LHS = Creator.make<EnableIfAttr>(Args1);
    Node const *RHS = Creator.make<EnableIfAttr>(Args1);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different args
    Node const *LHS = Creator.make<EnableIfAttr>(Args1);
    Node const *RHS = Creator.make<EnableIfAttr>(Args2);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_PointerType) {
  ManglingNodeCreator Creator;

  auto *Name1 = Creator.make<NameType>("double");
  auto *Name2 = Creator.make<NameType>("int");

  {
    // Equal
    Node const *LHS = Creator.make<itanium_demangle::PointerType>(Name1);
    Node const *RHS = Creator.make<itanium_demangle::PointerType>(Name1);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different types
    Node const *LHS = Creator.make<itanium_demangle::PointerType>(Name1);
    Node const *RHS = Creator.make<itanium_demangle::PointerType>(Name2);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_PointerToMemberType) {
  ManglingNodeCreator Creator;

  auto *Class1 = Creator.make<NameType>("Foo");
  auto *Class2 = Creator.make<NameType>("Bar");

  auto *Member1 = Creator.make<NameType>("int");
  auto *Member2 = Creator.make<NameType>("double");

  {
    // Equal
    Node const *LHS = Creator.make<PointerToMemberType>(Class1, Member1);
    Node const *RHS = Creator.make<PointerToMemberType>(Class1, Member1);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different class types
    Node const *LHS = Creator.make<PointerToMemberType>(Class1, Member1);
    Node const *RHS = Creator.make<PointerToMemberType>(Class2, Member1);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different member types
    Node const *LHS = Creator.make<PointerToMemberType>(Class1, Member1);
    Node const *RHS = Creator.make<PointerToMemberType>(Class1, Member2);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_ArrayType) {
  ManglingNodeCreator Creator;

  auto *Class1 = Creator.make<NameType>("Foo");
  auto *Class2 = Creator.make<NameType>("Bar");

  auto *Dim1 = Creator.make<NameType>("32");
  auto *Dim2 = Creator.make<NameType>("64");

  {
    // Equal
    Node const *LHS = Creator.make<itanium_demangle::ArrayType>(Class1, nullptr);
    Node const *RHS = Creator.make<itanium_demangle::ArrayType>(Class1, nullptr);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different class type
    Node const *LHS = Creator.make<itanium_demangle::ArrayType>(Class1, nullptr);
    Node const *RHS = Creator.make<itanium_demangle::ArrayType>(Class2, nullptr);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different dimensions
    Node const *LHS = Creator.make<itanium_demangle::ArrayType>(Class1, nullptr);
    Node const *RHS = Creator.make<itanium_demangle::ArrayType>(Class1, Dim2);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different dimensions
    Node const *LHS = Creator.make<itanium_demangle::ArrayType>(Class1, Dim1);
    Node const *RHS = Creator.make<itanium_demangle::ArrayType>(Class1, nullptr);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different dimensions
    Node const *LHS = Creator.make<itanium_demangle::ArrayType>(Class1, Dim1);
    Node const *RHS = Creator.make<itanium_demangle::ArrayType>(Class1, Dim2);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_DynamicExceptionSpec) {
  ManglingNodeCreator Creator;

  std::vector<Node *> Types1{Creator.make<NameType>("false")};
  std::vector<Node *> Types2{Creator.make<NameType>("true")};

  NodeArray Operand1 = Creator.makeNodeArray(Types1.cbegin(), Types1.cend());
  NodeArray Operand2 = Creator.makeNodeArray(Types2.cbegin(), Types2.cend());

  {
    // Equal
    Node const *LHS = Creator.make<DynamicExceptionSpec>(Operand1);
    Node const *RHS = Creator.make<DynamicExceptionSpec>(Operand1);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different operand
    Node const *LHS = Creator.make<DynamicExceptionSpec>(Operand1);
    Node const *RHS = Creator.make<DynamicExceptionSpec>(Operand2);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_FunctionType) {
  ManglingNodeCreator Creator;

  auto const *Ret1 = Creator.make<NameType>("double");
  auto const *Ret2 = Creator.make<NameType>("int");

  std::vector<Node *> Ex1{Creator.make<NameType>("false")};
  std::vector<Node *> Ex2{Creator.make<NameType>("true")};

  auto *ExSpec1 = Creator.make<DynamicExceptionSpec>(
          Creator.makeNodeArray(Ex1.cbegin(), Ex1.cend()));

  auto *ExSpec2 = Creator.make<DynamicExceptionSpec>(
          Creator.makeNodeArray(Ex2.cbegin(), Ex2.cend()));

  std::vector<Node *> Params1{Creator.make<NameType>("double"),
                              Creator.make<NameType>("int")};

  std::vector<Node *> Params2{Creator.make<NameType>("int")};

  NodeArray Args1 = Creator.makeNodeArray(Params1.cbegin(), Params1.cend());
  NodeArray Args2 = Creator.makeNodeArray(Params2.cbegin(), Params2.cend());

  {
    // Equal
    Node const* LHS = Creator.make<itanium_demangle::FunctionType>(
            Ret1, Args1, Qualifiers::QualConst, FunctionRefQual::FrefQualLValue, nullptr);

    Node const* RHS = Creator.make<itanium_demangle::FunctionType>(
            Ret1, Args1, Qualifiers::QualConst, FunctionRefQual::FrefQualLValue, nullptr);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different return type
    Node const* LHS = Creator.make<itanium_demangle::FunctionType>(
            Ret1, Args1, Qualifiers::QualConst, FunctionRefQual::FrefQualLValue, nullptr);

    Node const* RHS = Creator.make<itanium_demangle::FunctionType>(
            Ret2, Args1, Qualifiers::QualConst, FunctionRefQual::FrefQualLValue, nullptr);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different args
    Node const* LHS = Creator.make<itanium_demangle::FunctionType>(
            Ret1, Args1, Qualifiers::QualConst, FunctionRefQual::FrefQualLValue, nullptr);

    Node const* RHS = Creator.make<itanium_demangle::FunctionType>(
            Ret1, Args2, Qualifiers::QualConst, FunctionRefQual::FrefQualLValue, nullptr);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different qualifiers
    Node const* LHS = Creator.make<itanium_demangle::FunctionType>(
            Ret1, Args1, Qualifiers::QualConst, FunctionRefQual::FrefQualLValue, nullptr);

    Node const* RHS = Creator.make<itanium_demangle::FunctionType>(
            Ret1, Args1, Qualifiers::QualRestrict, FunctionRefQual::FrefQualLValue, nullptr);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different references qualifiers
    Node const* LHS = Creator.make<itanium_demangle::FunctionType>(
            Ret1, Args1, Qualifiers::QualConst, FunctionRefQual::FrefQualLValue, nullptr);

    Node const* RHS = Creator.make<itanium_demangle::FunctionType>(
            Ret1, Args1, Qualifiers::QualConst, FunctionRefQual::FrefQualNone, nullptr);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different exception spec
    Node const* LHS = Creator.make<itanium_demangle::FunctionType>(
            Ret1, Args1, Qualifiers::QualConst, FunctionRefQual::FrefQualLValue, nullptr);

    Node const* RHS = Creator.make<itanium_demangle::FunctionType>(
            Ret1, Args1, Qualifiers::QualConst, FunctionRefQual::FrefQualLValue, ExSpec1);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different exception spec
    Node const* LHS = Creator.make<itanium_demangle::FunctionType>(
            Ret1, Args1, Qualifiers::QualConst, FunctionRefQual::FrefQualLValue, nullptr);

    Node const* RHS = Creator.make<itanium_demangle::FunctionType>(
            Ret1, Args1, Qualifiers::QualConst, FunctionRefQual::FrefQualLValue, ExSpec2);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different exception spec
    Node const* LHS = Creator.make<itanium_demangle::FunctionType>(
            Ret1, Args1, Qualifiers::QualConst, FunctionRefQual::FrefQualLValue, ExSpec1);

    Node const* RHS = Creator.make<itanium_demangle::FunctionType>(
            Ret1, Args1, Qualifiers::QualConst, FunctionRefQual::FrefQualLValue, ExSpec2);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_FunctionEncoding) {
  ManglingNodeCreator Creator;

  auto const *Name1 = Creator.make<NameType>("Foo");
  auto const *Name2 = Creator.make<NameType>("Bar");

  auto const *Ret1 = Creator.make<NameType>("double");
  auto const *Ret2 = Creator.make<NameType>("int");

  std::vector<Node *> Types1{Creator.make<NameType>("double")};
  std::vector<Node *> Types2{Creator.make<NameType>("int")};

  NodeArray Args1 = Creator.makeNodeArray(Types1.cbegin(), Types1.cend());
  NodeArray Args2 = Creator.makeNodeArray(Types2.cbegin(), Types2.cend());

  auto const *Attr1 = Creator.make<EnableIfAttr>(Args1);
  auto const *Attr2 = Creator.make<EnableIfAttr>(Args2);

  {
    // Equal
    Node const* LHS = Creator.make<FunctionEncoding>(
            Ret1, Name1, Args1, Attr1, Qualifiers::QualConst, FunctionRefQual::FrefQualLValue);

    Node const* RHS = Creator.make<FunctionEncoding>(
            Ret1, Name1, Args1, Attr1, Qualifiers::QualConst, FunctionRefQual::FrefQualLValue);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different return type
    Node const* LHS = Creator.make<FunctionEncoding>(
            Ret1, Name1, Args1, Attr1, Qualifiers::QualConst, FunctionRefQual::FrefQualLValue);

    Node const* RHS = Creator.make<FunctionEncoding>(
            Ret2, Name1, Args1, Attr1, Qualifiers::QualConst, FunctionRefQual::FrefQualLValue);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different name
    Node const* LHS = Creator.make<FunctionEncoding>(
            Ret1, Name1, Args1, Attr1, Qualifiers::QualConst, FunctionRefQual::FrefQualLValue);

    Node const* RHS = Creator.make<FunctionEncoding>(
            Ret1, Name2, Args1, Attr1, Qualifiers::QualConst, FunctionRefQual::FrefQualLValue);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different args
    Node const* LHS = Creator.make<FunctionEncoding>(
            Ret1, Name1, Args1, Attr1, Qualifiers::QualConst, FunctionRefQual::FrefQualLValue);

    Node const* RHS = Creator.make<FunctionEncoding>(
            Ret1, Name1, Args2, Attr1, Qualifiers::QualConst, FunctionRefQual::FrefQualLValue);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different attrs
    Node const* LHS = Creator.make<FunctionEncoding>(
            Ret1, Name1, Args1, nullptr, Qualifiers::QualConst, FunctionRefQual::FrefQualLValue);

    Node const* RHS = Creator.make<FunctionEncoding>(
            Ret1, Name1, Args1, Attr2, Qualifiers::QualConst, FunctionRefQual::FrefQualLValue);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different attrs
    Node const* LHS = Creator.make<FunctionEncoding>(
            Ret1, Name1, Args1, Attr1, Qualifiers::QualConst, FunctionRefQual::FrefQualLValue);

    Node const* RHS = Creator.make<FunctionEncoding>(
            Ret1, Name1, Args1, nullptr, Qualifiers::QualConst, FunctionRefQual::FrefQualLValue);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different attrs
    Node const* LHS = Creator.make<FunctionEncoding>(
            Ret1, Name1, Args1, Attr1, Qualifiers::QualConst, FunctionRefQual::FrefQualLValue);

    Node const* RHS = Creator.make<FunctionEncoding>(
            Ret1, Name1, Args1, Attr2, Qualifiers::QualConst, FunctionRefQual::FrefQualLValue);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different qualifiers
    Node const* LHS = Creator.make<FunctionEncoding>(
            Ret1, Name1, Args1, Attr1, Qualifiers::QualConst, FunctionRefQual::FrefQualLValue);

    Node const* RHS = Creator.make<FunctionEncoding>(
            Ret1, Name1, Args1, Attr1, Qualifiers::QualRestrict, FunctionRefQual::FrefQualLValue);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different reference-qualifiers
    Node const* LHS = Creator.make<FunctionEncoding>(
            Ret1, Name1, Args1, Attr1, Qualifiers::QualConst, FunctionRefQual::FrefQualLValue);

    Node const* RHS = Creator.make<FunctionEncoding>(
            Ret1, Name1, Args1, Attr1, Qualifiers::QualConst, FunctionRefQual::FrefQualRValue);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_NoexceptSpec) {
  ManglingNodeCreator Creator;
  
  auto *ExSpec1 = Creator.make<NameType>("false");
  auto *ExSpec2 = Creator.make<NameType>("true");

  {
    // Equal
    Node const *LHS = Creator.make<NoexceptSpec>(ExSpec1);
    Node const *RHS = Creator.make<NoexceptSpec>(ExSpec1);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different operand
    Node const *LHS = Creator.make<NoexceptSpec>(ExSpec1);
    Node const *RHS = Creator.make<NoexceptSpec>(ExSpec2);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_ModuleEntity) {
  ManglingNodeCreator Creator;

  auto *Name1 = Creator.make<NameType>("Foo");
  auto *Name2 = Creator.make<NameType>("Bar");

  ModuleName *Mod1 =
      static_cast<ModuleName *>(Creator.make<ModuleName>(nullptr, Name1, true));

  ModuleName *Mod2 =
      static_cast<ModuleName *>(Creator.make<ModuleName>(nullptr, Name2, true));

  {
    // Equal
    Node const *LHS = Creator.make<ModuleEntity>(Mod1, Creator.make<NameType>("Foo"));
    Node const *RHS = Creator.make<ModuleEntity>(Mod1, Creator.make<NameType>("Foo"));
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different ModuleNames
    Node const *LHS = Creator.make<ModuleEntity>(Mod1, Creator.make<NameType>("Foo"));
    Node const *RHS = Creator.make<ModuleEntity>(Mod2, Creator.make<NameType>("Foo"));
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different enitity names
    Node const *LHS = Creator.make<ModuleEntity>(Mod1, Creator.make<NameType>("Foo"));
    Node const *RHS = Creator.make<ModuleEntity>(Mod1, Creator.make<NameType>("Bar"));
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_VectorType) {
  ManglingNodeCreator Creator;

  auto *Class1 = Creator.make<NameType>("Foo");
  auto *Class2 = Creator.make<NameType>("Bar");

  auto *Dim1 = Creator.make<NameType>("32");
  auto *Dim2 = Creator.make<NameType>("64");

  {
    // Equal
    Node const *LHS = Creator.make<itanium_demangle::VectorType>(Class1, nullptr);
    Node const *RHS = Creator.make<itanium_demangle::VectorType>(Class1, nullptr);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different class type
    Node const *LHS = Creator.make<itanium_demangle::VectorType>(Class1, nullptr);
    Node const *RHS = Creator.make<itanium_demangle::VectorType>(Class2, nullptr);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different dimensions
    Node const *LHS = Creator.make<itanium_demangle::VectorType>(Class1, nullptr);
    Node const *RHS = Creator.make<itanium_demangle::VectorType>(Class1, Dim2);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different dimensions
    Node const *LHS = Creator.make<itanium_demangle::VectorType>(Class1, Dim1);
    Node const *RHS = Creator.make<itanium_demangle::VectorType>(Class1, nullptr);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different dimensions
    Node const *LHS = Creator.make<itanium_demangle::VectorType>(Class1, Dim1);
    Node const *RHS = Creator.make<itanium_demangle::VectorType>(Class1, Dim2);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_PixelVectorType) {
  ManglingNodeCreator Creator;

  auto *Dim1 = Creator.make<NameType>("32");
  auto *Dim2 = Creator.make<NameType>("64");

  {
    // Equal
    Node const *LHS = Creator.make<PixelVectorType>(Dim1);
    Node const *RHS = Creator.make<PixelVectorType>(Dim1);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different class type
    Node const *LHS = Creator.make<PixelVectorType>(Dim1);
    Node const *RHS = Creator.make<PixelVectorType>(Dim2);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_BinaryFPType) {
  ManglingNodeCreator Creator;

  auto *Dim1 = Creator.make<NameType>("32");
  auto *Dim2 = Creator.make<NameType>("64");

  {
    // Equal
    Node const *LHS = Creator.make<BinaryFPType>(Dim1);
    Node const *RHS = Creator.make<BinaryFPType>(Dim1);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different class type
    Node const *LHS = Creator.make<BinaryFPType>(Dim1);
    Node const *RHS = Creator.make<BinaryFPType>(Dim2);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_SyntheticTemplateParamName) {
  ManglingNodeCreator Creator;

  {
    // Equal
    Node const *LHS = Creator.make<SyntheticTemplateParamName>(TemplateParamKind::Template, 0);
    Node const *RHS = Creator.make<SyntheticTemplateParamName>(TemplateParamKind::Template, 0);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different index
    Node const *LHS = Creator.make<SyntheticTemplateParamName>(TemplateParamKind::Template, 0);
    Node const *RHS = Creator.make<SyntheticTemplateParamName>(TemplateParamKind::Template, 1);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different param kind
    Node const *LHS = Creator.make<SyntheticTemplateParamName>(TemplateParamKind::Template, 0);
    Node const *RHS = Creator.make<SyntheticTemplateParamName>(TemplateParamKind::NonType, 0);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_TypeTemplateParamDecl ) {
  ManglingNodeCreator Creator;

  auto *Name1 = Creator.make<NameType>("int");
  auto *Name2 = Creator.make<NameType>("double");

  {
    // Equal
    Node const *LHS = Creator.make<TypeTemplateParamDecl>(Name1);
    Node const *RHS = Creator.make<TypeTemplateParamDecl>(Name1);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different names
    Node const *LHS = Creator.make<TypeTemplateParamDecl>(Name1);
    Node const *RHS = Creator.make<TypeTemplateParamDecl>(Name2);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_NonTypeTemplateParamDecl) {
  ManglingNodeCreator Creator;

  auto *Name1 = Creator.make<NameType>("Foo");
  auto *Name2 = Creator.make<NameType>("Bar");

  auto *Type1 = Creator.make<NameType>("int");
  auto *Type2 = Creator.make<NameType>("double");

  {
    // Equal
    Node const *LHS = Creator.make<NonTypeTemplateParamDecl>(Name1, Type1);
    Node const *RHS = Creator.make<NonTypeTemplateParamDecl>(Name1, Type1);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different names
    Node const *LHS = Creator.make<NonTypeTemplateParamDecl>(Name1, Type1);
    Node const *RHS = Creator.make<NonTypeTemplateParamDecl>(Name2, Type1);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different types
    Node const *LHS = Creator.make<NonTypeTemplateParamDecl>(Name1, Type1);
    Node const *RHS = Creator.make<NonTypeTemplateParamDecl>(Name1, Type2);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_TemplateTemplateParamDecl) {
  ManglingNodeCreator Creator;

  auto *Name1 = Creator.make<NameType>("Foo");
  auto *Name2 = Creator.make<NameType>("Bar");

  std::vector<Node *> Params1{Creator.make<NameType>("double"),
                              Creator.make<NameType>("int")};

  std::vector<Node *> Params2{Creator.make<NameType>("int")};

  NodeArray Args1 = Creator.makeNodeArray(Params1.cbegin(), Params1.cend());
  NodeArray Args2 = Creator.makeNodeArray(Params2.cbegin(), Params2.cend());

  {
    // Equal
    Node const *LHS = Creator.make<TemplateTemplateParamDecl>(Name1, Args1);
    Node const *RHS = Creator.make<TemplateTemplateParamDecl>(Name1, Args1);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different names
    Node const *LHS = Creator.make<TemplateTemplateParamDecl>(Name1, Args1);
    Node const *RHS = Creator.make<TemplateTemplateParamDecl>(Name2, Args1);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different args
    Node const *LHS = Creator.make<TemplateTemplateParamDecl>(Name1, Args1);
    Node const *RHS = Creator.make<TemplateTemplateParamDecl>(Name1, Args2);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_TemplateParamPackDecl) {
  ManglingNodeCreator Creator;

  std::vector<Node *> Params1{Creator.make<NameType>("double"),
                              Creator.make<NameType>("int")};

  std::vector<Node *> Params2{Creator.make<NameType>("int")};

  auto *Args1 = Creator.make<NodeArrayNode>(
          Creator.makeNodeArray(Params1.cbegin(), Params1.cend()));

  auto *Args2 = Creator.make<NodeArrayNode>(
          Creator.makeNodeArray(Params2.cbegin(), Params2.cend()));

  {
    // Equal
    Node const *LHS = Creator.make<TemplateParamPackDecl>(Args1);
    Node const *RHS = Creator.make<TemplateParamPackDecl>(Args1);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different args
    Node const *LHS = Creator.make<TemplateParamPackDecl>(Args1);
    Node const *RHS = Creator.make<TemplateParamPackDecl>(Args2);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_BracedExpr) {
  ManglingNodeCreator Creator;

  auto* Expr1 = Creator.make<NameType>("Foo");
  auto* Expr2 = Creator.make<NameType>("Bar");

  auto* Init1 = Creator.make<NameType>("var1");
  auto* Init2 = Creator.make<NameType>("var2");

  {
    // Equal
    Node const *LHS = Creator.make<BracedExpr>(Expr1, Init1, false);
    Node const *RHS = Creator.make<BracedExpr>(Expr1, Init1, false);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different expressions
    Node const *LHS = Creator.make<BracedExpr>(Expr1, Init1, false);
    Node const *RHS = Creator.make<BracedExpr>(Expr2, Init1, false);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different init
    Node const *LHS = Creator.make<BracedExpr>(Expr1, Init1, false);
    Node const *RHS = Creator.make<BracedExpr>(Expr1, Init2, false);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different flags
    Node const *LHS = Creator.make<BracedExpr>(Expr1, Init1, false);
    Node const *RHS = Creator.make<BracedExpr>(Expr1, Init1, true);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_FoldExpr) {
  ManglingNodeCreator Creator;

  auto* Expr1 = Creator.make<NameType>("Foo");
  auto* Expr2 = Creator.make<NameType>("Bar");

  auto* Init1 = Creator.make<NameType>("var1");
  auto* Init2 = Creator.make<NameType>("var2");

  {
    // Equal
    Node const *LHS = Creator.make<FoldExpr>(false, "<<", Expr1, Init1);
    Node const *RHS = Creator.make<FoldExpr>(false, "<<", Expr1, Init1);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different flags
    Node const *LHS = Creator.make<FoldExpr>(false, "<<", Expr1, Init1);
    Node const *RHS = Creator.make<FoldExpr>(true, "<<", Expr1, Init1);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different operator
    Node const *LHS = Creator.make<FoldExpr>(false, "<<", Expr1, Init1);
    Node const *RHS = Creator.make<FoldExpr>(false, ">>", Expr1, Init1);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different pack
    Node const *LHS = Creator.make<FoldExpr>(false, "<<", Expr1, Init1);
    Node const *RHS = Creator.make<FoldExpr>(false, "<<", nullptr, Init1);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different pack
    Node const *LHS = Creator.make<FoldExpr>(false, "<<", nullptr, Init1);
    Node const *RHS = Creator.make<FoldExpr>(false, "<<", Expr2, Init1);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different pack
    Node const *LHS = Creator.make<FoldExpr>(false, "<<", Expr1, Init1);
    Node const *RHS = Creator.make<FoldExpr>(false, "<<", Expr2, Init1);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different pack
    Node const *LHS = Creator.make<FoldExpr>(false, "<<", Expr1, Init1);
    Node const *RHS = Creator.make<FoldExpr>(false, "<<", Expr1, nullptr);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different pack
    Node const *LHS = Creator.make<FoldExpr>(false, "<<", Expr1, nullptr);
    Node const *RHS = Creator.make<FoldExpr>(false, "<<", Expr1, Init2);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different pack
    Node const *LHS = Creator.make<FoldExpr>(false, "<<", Expr1, Init1);
    Node const *RHS = Creator.make<FoldExpr>(false, "<<", Expr1, Init2);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_ThrowExpr) {
  ManglingNodeCreator Creator;
  
  auto *ExSpec1 = Creator.make<NameType>("false");
  auto *ExSpec2 = Creator.make<NameType>("true");

  {
    // Equal
    Node const *LHS = Creator.make<ThrowExpr>(ExSpec1);
    Node const *RHS = Creator.make<ThrowExpr>(ExSpec1);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different operand
    Node const *LHS = Creator.make<NoexceptSpec>(ExSpec1);
    Node const *RHS = Creator.make<NoexceptSpec>(ExSpec2);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_BoolExpr) {
  ManglingNodeCreator Creator;
  
  {
    // Equal
    Node const *LHS = Creator.make<BoolExpr>(false);
    Node const *RHS = Creator.make<BoolExpr>(false);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different operand
    Node const *LHS = Creator.make<BoolExpr>(false);
    Node const *RHS = Creator.make<BoolExpr>(true);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_MemberExpr) {
  ManglingNodeCreator Creator;

  auto *Left1 = Creator.make<NameType>("Foo");
  auto *Left2 = Creator.make<NameType>("Bar");

  auto *Right1 = Creator.make<NameType>("Qux");
  auto *Right2 = Creator.make<NameType>("Quux");

  {
    // Equal
    Node const *LHS = Creator.make<MemberExpr>(Left1, "==", Right1, Node::Prec::Equality);
    Node const *RHS = Creator.make<MemberExpr>(Left1, "==", Right1, Node::Prec::Equality);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different LHS
    Node const *LHS = Creator.make<MemberExpr>(Left1, "==", Right1, Node::Prec::Equality);
    Node const *RHS = Creator.make<MemberExpr>(Left2, "==", Right1, Node::Prec::Equality);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different RHS
    Node const *LHS = Creator.make<MemberExpr>(Left1, "==", Right1, Node::Prec::Equality);
    Node const *RHS = Creator.make<MemberExpr>(Left1, "==", Right2, Node::Prec::Equality);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different kind
    Node const *LHS = Creator.make<MemberExpr>(Left1, "==", Right1, Node::Prec::Equality);
    Node const *RHS = Creator.make<MemberExpr>(Left1, "+=", Right1, Node::Prec::Equality);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_CastExpr) {
  ManglingNodeCreator Creator;

  auto *From1 = Creator.make<NameType>("Qux");
  auto *From2 = Creator.make<NameType>("Quux");

  auto *To1 = Creator.make<NameType>("Foo");
  auto *To2 = Creator.make<NameType>("Bar");

  {
    // Equal
    Node const *LHS = Creator.make<CastExpr>("static_cast", From1, To1, Node::Prec::Cast);
    Node const *RHS = Creator.make<CastExpr>("static_cast", From1, To1, Node::Prec::Cast);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different from type
    Node const *LHS = Creator.make<CastExpr>("static_cast", From1, To1, Node::Prec::Cast);
    Node const *RHS = Creator.make<CastExpr>("static_cast", From2, To1, Node::Prec::Cast);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different to type
    Node const *LHS = Creator.make<CastExpr>("static_cast", From1, To1, Node::Prec::Cast);
    Node const *RHS = Creator.make<CastExpr>("static_cast", From1, To2, Node::Prec::Cast);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different cast-kind
    Node const *LHS = Creator.make<CastExpr>("satic_cast", From1, To1, Node::Prec::Cast);
    Node const *RHS = Creator.make<CastExpr>("dynamic_cast", From1, To1, Node::Prec::Cast);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_ForwardTemplateReference) {
  ManglingNodeCreator Creator;

  auto *Type1 = Creator.make<NameType>("Foo");
  auto *Type2 = Creator.make<NameType>("Bar");

  {
    // Equal
    Node const *LHS = Creator.make<ForwardTemplateReference>(0);
    Node const *RHS = Creator.make<ForwardTemplateReference>(0);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Equal
    Node *LHS = Creator.make<ForwardTemplateReference>(0);
    Node *RHS = Creator.make<ForwardTemplateReference>(0);
    static_cast<ForwardTemplateReference*>(LHS)->Ref = Type1;
    static_cast<ForwardTemplateReference*>(RHS)->Ref = Type1;
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different index
    Node *LHS = Creator.make<ForwardTemplateReference>(0);
    Node *RHS = Creator.make<ForwardTemplateReference>(1);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different Ref
    Node *LHS = Creator.make<ForwardTemplateReference>(0);
    Node *RHS = Creator.make<ForwardTemplateReference>(0);
    static_cast<ForwardTemplateReference*>(LHS)->Ref = Type1;
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different Ref
    Node *LHS = Creator.make<ForwardTemplateReference>(0);
    Node *RHS = Creator.make<ForwardTemplateReference>(0);
    static_cast<ForwardTemplateReference*>(RHS)->Ref = Type2;
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different Ref
    Node *LHS = Creator.make<ForwardTemplateReference>(0);
    Node *RHS = Creator.make<ForwardTemplateReference>(0);
    static_cast<ForwardTemplateReference*>(LHS)->Ref = Type1;
    static_cast<ForwardTemplateReference*>(RHS)->Ref = Type2;
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_GlobalQualifiedName) {
  ManglingNodeCreator Creator;

  auto *Name1 = Creator.make<NameType>("Foo");
  auto *Name2 = Creator.make<NameType>("Bar");

  {
    // Equal
    Node *LHS = Creator.make<GlobalQualifiedName>(Name1);
    Node *RHS = Creator.make<GlobalQualifiedName>(Name1);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different names
    Node *LHS = Creator.make<GlobalQualifiedName>(Name1);
    Node *RHS = Creator.make<GlobalQualifiedName>(Name2);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_BinaryExpr) {
  ManglingNodeCreator Creator;

  auto *Name1 = Creator.make<NameType>("Foo");
  auto *Name2 = Creator.make<NameType>("Bar");

  {
    // Equal
    Node *LHS = Creator.make<BinaryExpr>(Name1, "==", Name2, Node::Prec::Equality);
    Node *RHS = Creator.make<BinaryExpr>(Name1, "==", Name2, Node::Prec::Equality);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different left node
    Node *LHS = Creator.make<BinaryExpr>(Name1, "==", Name2, Node::Prec::Equality);
    Node *RHS = Creator.make<BinaryExpr>(Name2, "==", Name2, Node::Prec::Equality);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different operator
    Node *LHS = Creator.make<BinaryExpr>(Name1, "==", Name2, Node::Prec::Equality);
    Node *RHS = Creator.make<BinaryExpr>(Name1, "!=", Name2, Node::Prec::Equality);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different right node
    Node *LHS = Creator.make<BinaryExpr>(Name1, "==", Name2, Node::Prec::Equality);
    Node *RHS = Creator.make<BinaryExpr>(Name1, "==", Name1, Node::Prec::Equality);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_ArraySubscriptExpr) {
  ManglingNodeCreator Creator;

  auto *Name1 = Creator.make<NameType>("foo");
  auto *Name2 = Creator.make<NameType>("bar");

  auto *Subscript1 = Creator.make<NameType>("0");
  auto *Subscript2 = Creator.make<NameType>("1");

  {
    // Equal
    Node *LHS = Creator.make<ArraySubscriptExpr>(Name1, Subscript1, Node::Prec::Default);
    Node *RHS = Creator.make<ArraySubscriptExpr>(Name1, Subscript1, Node::Prec::Default);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different names
    Node *LHS = Creator.make<ArraySubscriptExpr>(Name1, Subscript1, Node::Prec::Default);
    Node *RHS = Creator.make<ArraySubscriptExpr>(Name2, Subscript1, Node::Prec::Default);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different subscripts
    Node *LHS = Creator.make<ArraySubscriptExpr>(Name1, Subscript1, Node::Prec::Default);
    Node *RHS = Creator.make<ArraySubscriptExpr>(Name1, Subscript2, Node::Prec::Default);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_PostfixExpr) {
  ManglingNodeCreator Creator;

  auto *Name1 = Creator.make<NameType>("foo");
  auto *Name2 = Creator.make<NameType>("bar");

  {
    // Equal
    Node *LHS = Creator.make<PostfixExpr>(Name1, "++", Node::Prec::Default);
    Node *RHS = Creator.make<PostfixExpr>(Name1, "++", Node::Prec::Default);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different names
    Node *LHS = Creator.make<PostfixExpr>(Name1, "++", Node::Prec::Default);
    Node *RHS = Creator.make<PostfixExpr>(Name2, "++", Node::Prec::Default);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different operators
    Node *LHS = Creator.make<PostfixExpr>(Name1, "++", Node::Prec::Default);
    Node *RHS = Creator.make<PostfixExpr>(Name1, "--", Node::Prec::Default);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_ConditionalExpr) {
  ManglingNodeCreator Creator;

  auto *Cond1 = Creator.make<NameType>("cond1");
  auto *Cond2 = Creator.make<NameType>("cond2");

  auto *Then1 = Creator.make<NameType>("then1");
  auto *Then2 = Creator.make<NameType>("then2");

  auto *Else1 = Creator.make<NameType>("else1");
  auto *Else2 = Creator.make<NameType>("else2");

  {
    // Equal
    Node *LHS = Creator.make<ConditionalExpr>(Cond1, Then1, Else1, Node::Prec::Conditional);
    Node *RHS = Creator.make<ConditionalExpr>(Cond1, Then1, Else1, Node::Prec::Conditional);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different condition
    Node *LHS = Creator.make<ConditionalExpr>(Cond1, Then1, Else1, Node::Prec::Conditional);
    Node *RHS = Creator.make<ConditionalExpr>(Cond2, Then1, Else1, Node::Prec::Conditional);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different then-expression
    Node *LHS = Creator.make<ConditionalExpr>(Cond1, Then1, Else1, Node::Prec::Conditional);
    Node *RHS = Creator.make<ConditionalExpr>(Cond1, Then2, Else1, Node::Prec::Conditional);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different else-expression
    Node *LHS = Creator.make<ConditionalExpr>(Cond1, Then1, Else1, Node::Prec::Conditional);
    Node *RHS = Creator.make<ConditionalExpr>(Cond1, Then1, Else2, Node::Prec::Conditional);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_EnclosingExpr) {
  ManglingNodeCreator Creator;

  auto *Name1 = Creator.make<NameType>("foo");
  auto *Name2 = Creator.make<NameType>("bar");

  {
    // Equal
    Node *LHS = Creator.make<EnclosingExpr>("[", Name1);
    Node *RHS = Creator.make<EnclosingExpr>("[", Name1);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different left enclosing expression
    Node *LHS = Creator.make<EnclosingExpr>("[", Name1);
    Node *RHS = Creator.make<EnclosingExpr>("{", Name1);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different names
    Node *LHS = Creator.make<EnclosingExpr>("[", Name1);
    Node *RHS = Creator.make<EnclosingExpr>("[", Name2);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_CallExpr) {
  ManglingNodeCreator Creator;

  auto *Name1 = Creator.make<NameType>("foo");
  auto *Name2 = Creator.make<NameType>("bar");

  std::vector<Node *> Params1{Creator.make<NameType>("double"),
                              Creator.make<NameType>("int")};

  std::vector<Node *> Params2{Creator.make<NameType>("int")};

  NodeArray Args1 = Creator.makeNodeArray(Params1.cbegin(), Params1.cend());
  NodeArray Args2 = Creator.makeNodeArray(Params2.cbegin(), Params2.cend());

  {
    // Equal
    Node *LHS = Creator.make<CallExpr>(Name1, Args1, Node::Prec::Default);
    Node *RHS = Creator.make<CallExpr>(Name1, Args1, Node::Prec::Default);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different names
    Node *LHS = Creator.make<CallExpr>(Name1, Args1, Node::Prec::Default);
    Node *RHS = Creator.make<CallExpr>(Name2, Args1, Node::Prec::Default);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different args
    Node *LHS = Creator.make<CallExpr>(Name1, Args1, Node::Prec::Default);
    Node *RHS = Creator.make<CallExpr>(Name1, Args2, Node::Prec::Default);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_NewExpr) {
  ManglingNodeCreator Creator;

  auto *Type1 = Creator.make<NameType>("Foo");
  auto *Type2 = Creator.make<NameType>("Bar");

  std::vector<Node *> Params1{Creator.make<NameType>("double"),
                              Creator.make<NameType>("int")};

  std::vector<Node *> Params2{Creator.make<NameType>("int")};

  NodeArray Exprs1 = Creator.makeNodeArray(Params1.cbegin(), Params1.cend());
  NodeArray Exprs2 = Creator.makeNodeArray(Params2.cbegin(), Params2.cend());

  {
    // Equal
    Node *LHS = Creator.make<NewExpr>(Exprs1, Type1, Exprs1, true, true, Node::Prec::Default);
    Node *RHS = Creator.make<NewExpr>(Exprs1, Type1, Exprs1, true, true, Node::Prec::Default);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different init expr list
    Node *LHS = Creator.make<NewExpr>(Exprs1, Type1, Exprs1, true, true, Node::Prec::Default);
    Node *RHS = Creator.make<NewExpr>(Exprs2, Type1, Exprs1, true, true, Node::Prec::Default);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different types
    Node *LHS = Creator.make<NewExpr>(Exprs1, Type1, Exprs1, true, true, Node::Prec::Default);
    Node *RHS = Creator.make<NewExpr>(Exprs1, Type2, Exprs1, true, true, Node::Prec::Default);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different args
    Node *LHS = Creator.make<NewExpr>(Exprs1, Type1, Exprs1, true, true, Node::Prec::Default);
    Node *RHS = Creator.make<NewExpr>(Exprs1, Type1, Exprs2, true, true, Node::Prec::Default);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different IsGlobal
    Node *LHS = Creator.make<NewExpr>(Exprs1, Type1, Exprs1, true, true, Node::Prec::Default);
    Node *RHS = Creator.make<NewExpr>(Exprs1, Type1, Exprs1, false, true, Node::Prec::Default);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different IsArray
    Node *LHS = Creator.make<NewExpr>(Exprs1, Type1, Exprs1, true, true, Node::Prec::Default);
    Node *RHS = Creator.make<NewExpr>(Exprs1, Type1, Exprs1, true, false, Node::Prec::Default);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}

TEST(ItaniumDemangle, Equality_DeleteExpr) {
  ManglingNodeCreator Creator;

  auto *Op1 = Creator.make<NameType>("Foo");
  auto *Op2 = Creator.make<NameType>("Bar");

  {
    // Equal
    Node *LHS = Creator.make<DeleteExpr>(Op1, true, true, Node::Prec::Default);
    Node *RHS = Creator.make<DeleteExpr>(Op1, true, true, Node::Prec::Default);
    ASSERT_TRUE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different operands
    Node *LHS = Creator.make<DeleteExpr>(Op1, true, true, Node::Prec::Default);
    Node *RHS = Creator.make<DeleteExpr>(Op2, true, true, Node::Prec::Default);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different IsGlobal
    Node *LHS = Creator.make<DeleteExpr>(Op1, true, true, Node::Prec::Default);
    Node *RHS = Creator.make<DeleteExpr>(Op2, false, true, Node::Prec::Default);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }

  {
    // Different IsArray
    Node *LHS = Creator.make<DeleteExpr>(Op1, true, true, Node::Prec::Default);
    Node *RHS = Creator.make<DeleteExpr>(Op1, true, false, Node::Prec::Default);
    ASSERT_FALSE(test_utils::compareNodes(LHS, RHS));
  }
}
