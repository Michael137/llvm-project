//===------------------ ItaniumDemangleTest.cpp ---------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Demangle/ItaniumDemangle.h"
#include "llvm/Support/Allocator.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <cstdlib>
#include <string_view>
#include <vector>

using namespace llvm;
using namespace llvm::itanium_demangle;

namespace {
class TestAllocator {
  BumpPtrAllocator Alloc;

public:
  void reset() { Alloc.Reset(); }

  template <typename T, typename... Args> T *makeNode(Args &&... args) {
    return new (Alloc.Allocate(sizeof(T), alignof(T)))
        T(std::forward<Args>(args)...);
  }

  void *allocateNodeArray(size_t sz) {
    return Alloc.Allocate(sizeof(Node *) * sz, alignof(Node *));
  }
};
} // namespace

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
  std::string_view SV = OB;
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
      std::string_view Name = N->getBaseName();
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

struct DemanglingPartsTestCase {
  const char *mangled;
  itanium_demangle::OutputBuffer::CxxNamePartsInfo expected_parts;
  llvm::StringRef basename;
  llvm::StringRef scope;
};

DemanglingPartsTestCase g_demangling_parts_test_cases[] = {
  // clang-format off
  { "_ZNVKO3BarIN2ns3QuxIiEEE1CIPFi3FooIS_IiES6_EEE6methodIS6_EENS5_IT_SC_E5InnerIiEESD_SD_",
    { .BasenameLocs = {92, 98}, .ScopeLocs = {36, 92}, .FunctionQualifiersLocs = { 158, 176 } },
    .basename = "method",
    .scope = "Bar<ns::Qux<int>>::C<int (*)(Foo<Bar<int>, Bar<int>>)>::"
  },
  { "_Z7getFuncIfEPFiiiET_",
    { .BasenameLocs = {6, 13}, .ScopeLocs = {6, 6}, .FunctionQualifiersLocs = { 38, 38 } },
    .basename = "getFunc",
    .scope = ""
  },
  { "_ZN1f1b1c1gEv",
    { .BasenameLocs = {9, 10}, .ScopeLocs = {0, 9}, .FunctionQualifiersLocs = { 12, 12 } },
    .basename = "g",
    .scope = "f::b::c::"
  },
  { "_ZN5test73fD1IiEEDTcmtlNS_1DEL_ZNS_1bEEEcvT__EES2_",
    { .BasenameLocs = {45, 48}, .ScopeLocs = {38, 45}, .FunctionQualifiersLocs = { 58, 58 } },
    .basename = "fD1",
    .scope = "test7::"
  },
  { "_ZN8nlohmann16json_abi_v3_11_310basic_jsonINSt3__13mapENS2_6vectorENS2_12basic_stringIcNS2_11char_traitsIcEENS2_9allocatorIcEEEEbxydS8_NS0_14adl_serializerENS4_IhNS8_IhEEEEvE5parseIRA29_KcEESE_OT_NS2_8functionIFbiNS0_6detail13parse_event_tERSE_EEEbb",
    { .BasenameLocs = {687, 692}, .ScopeLocs = {343, 687}, .FunctionQualifiersLocs = { 1174, 1174 } },
    .basename = "parse",
    .scope = "nlohmann::json_abi_v3_11_3::basic_json<std::__1::map, std::__1::vector, std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char>>, bool, long long, unsigned long long, double, std::__1::allocator, nlohmann::json_abi_v3_11_3::adl_serializer, std::__1::vector<unsigned char, std::__1::allocator<unsigned char>>, void>::"
  },
  { "_ZN8nlohmann16json_abi_v3_11_310basic_jsonINSt3__13mapENS2_6vectorENS2_12basic_stringIcNS2_11char_traitsIcEENS2_9allocatorIcEEEEbxydS8_NS0_14adl_serializerENS4_IhNS8_IhEEEEvEC1EDn",
    { .BasenameLocs = {344, 354}, .ScopeLocs = {0, 344}, .FunctionQualifiersLocs = { 370, 370 } },
    .basename = "basic_json",
    .scope = "nlohmann::json_abi_v3_11_3::basic_json<std::__1::map, std::__1::vector, std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char>>, bool, long long, unsigned long long, double, std::__1::allocator, nlohmann::json_abi_v3_11_3::adl_serializer, std::__1::vector<unsigned char, std::__1::allocator<unsigned char>>, void>::"
  }
  // clang-format on
};

struct DemanglingPartsTestFixture : public ::testing::TestWithParam<DemanglingPartsTestCase> {};

TEST_P(DemanglingPartsTestFixture, DemanglingParts) {
  const auto &[mangled, parts, basename, scope] = GetParam();

  ManglingParser<TestAllocator> Parser(mangled, mangled + ::strlen(mangled));

  const auto * Root = Parser.parse();

  ASSERT_NE(nullptr, Root);

  OutputBuffer OB;
  Root->print(OB);
  auto demangled = toString(OB);

  EXPECT_EQ(OB.PartsInfo.BasenameLocs, parts.BasenameLocs);
  EXPECT_EQ(OB.PartsInfo.ScopeLocs, parts.ScopeLocs);
  EXPECT_EQ(OB.PartsInfo.FunctionQualifiersLocs, parts.FunctionQualifiersLocs);

  auto get_part = [&](const std::pair<size_t, size_t> &loc) {
    return demangled.substr(loc.first, loc.second - loc.first);
  };

  EXPECT_EQ(get_part(OB.PartsInfo.BasenameLocs), basename);
  EXPECT_EQ(get_part(OB.PartsInfo.ScopeLocs), scope);
}

INSTANTIATE_TEST_SUITE_P(
        DemanglingPartsTests,
        DemanglingPartsTestFixture,
        ::testing::ValuesIn(g_demangling_parts_test_cases));
