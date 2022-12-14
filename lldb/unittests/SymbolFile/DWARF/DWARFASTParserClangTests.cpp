//===-- DWARFASTParserClangTests.cpp --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Plugins/Platform/MacOSX/PlatformMacOSX.h"
#include "Plugins/Platform/MacOSX/PlatformRemoteMacOSX.h"
#include "Plugins/SymbolFile/DWARF/DWARFASTParserClang.h"
#include "Plugins/SymbolFile/DWARF/DWARFCompileUnit.h"
#include "Plugins/SymbolFile/DWARF/DWARFDIE.h"
#include "TestingSupport/Symbol/ClangTestUtils.h"
#include "TestingSupport/Symbol/YAMLModuleTester.h"
#include "lldb/Core/Debugger.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::dwarf;

namespace {
static std::once_flag debugger_initialize_flag;

class DWARFASTParserClangTests : public testing::Test {
  void SetUp() override {
    HostInfo::Initialize();
    PlatformMacOSX::Initialize();
    std::call_once(debugger_initialize_flag,
                   []() { Debugger::Initialize(nullptr); });
    ArchSpec arch("x86_64-apple-macosx-");
    Platform::SetHostPlatform(
        PlatformRemoteMacOSX::CreateInstance(true, &arch));
  }
  void TearDown() override {
    PlatformMacOSX::Terminate();
    HostInfo::Terminate();
  }
};

class DWARFASTParserClangStub : public DWARFASTParserClang {
public:
  using DWARFASTParserClang::DWARFASTParserClang;
  using DWARFASTParserClang::LinkDeclContextToDIE;

  std::vector<const clang::DeclContext *> GetDeclContextToDIEMapKeys() {
    std::vector<const clang::DeclContext *> keys;
    for (const auto &it : m_decl_ctx_to_die)
      keys.push_back(it.first);
    return keys;
  }
};
} // namespace

// If your implementation needs to dereference the dummy pointers we are
// defining here, causing this test to fail, feel free to delete it.
TEST_F(DWARFASTParserClangTests,
       EnsureAllDIEsInDeclContextHaveBeenParsedParsesOnlyMatchingEntries) {

  /// Auxiliary debug info.
  const char *yamldata = R"(
--- !ELF
FileHeader:
  Class:   ELFCLASS64
  Data:    ELFDATA2LSB
  Type:    ET_EXEC
  Machine: EM_386
DWARF:
  debug_abbrev:
    - Table:
        - Code:            0x00000001
          Tag:             DW_TAG_compile_unit
          Children:        DW_CHILDREN_yes
          Attributes:
            - Attribute:       DW_AT_language
              Form:            DW_FORM_data2
        - Code:            0x00000002
          Tag:             DW_TAG_base_type
          Children:        DW_CHILDREN_no
          Attributes:
            - Attribute:       DW_AT_encoding
              Form:            DW_FORM_data1
            - Attribute:       DW_AT_byte_size
              Form:            DW_FORM_data1
  debug_info:
    - Version:         4
      AddrSize:        8
      Entries:
        - AbbrCode:        0x00000001
          Values:
            - Value:           0x000000000000000C
        - AbbrCode:        0x00000002
          Values:
            - Value:           0x0000000000000007 # DW_ATE_unsigned
            - Value:           0x0000000000000004
        - AbbrCode:        0x00000002
          Values:
            - Value:           0x0000000000000007 # DW_ATE_unsigned
            - Value:           0x0000000000000008
        - AbbrCode:        0x00000002
          Values:
            - Value:           0x0000000000000005 # DW_ATE_signed
            - Value:           0x0000000000000008
        - AbbrCode:        0x00000002
          Values:
            - Value:           0x0000000000000008 # DW_ATE_unsigned_char
            - Value:           0x0000000000000001
        - AbbrCode:        0x00000000
)";

  YAMLModuleTester t(yamldata);
  ASSERT_TRUE((bool)t.GetDwarfUnit());

  auto holder = std::make_unique<clang_utils::TypeSystemClangHolder>("ast");
  auto &ast_ctx = *holder->GetAST();

  DWARFASTParserClangStub ast_parser(ast_ctx);

  DWARFUnit *unit = t.GetDwarfUnit();
  const DWARFDebugInfoEntry *die_first = unit->DIE().GetDIE();
  const DWARFDebugInfoEntry *die_child0 = die_first->GetFirstChild();
  const DWARFDebugInfoEntry *die_child1 = die_child0->GetSibling();
  const DWARFDebugInfoEntry *die_child2 = die_child1->GetSibling();
  const DWARFDebugInfoEntry *die_child3 = die_child2->GetSibling();
  std::vector<DWARFDIE> dies = {
      DWARFDIE(unit, die_child0), DWARFDIE(unit, die_child1),
      DWARFDIE(unit, die_child2), DWARFDIE(unit, die_child3)};
  std::vector<clang::DeclContext *> decl_ctxs = {
      (clang::DeclContext *)1LL, (clang::DeclContext *)2LL,
      (clang::DeclContext *)2LL, (clang::DeclContext *)3LL};
  for (int i = 0; i < 4; ++i)
    ast_parser.LinkDeclContextToDIE(decl_ctxs[i], dies[i]);
  ast_parser.EnsureAllDIEsInDeclContextHaveBeenParsed(
      CompilerDeclContext(nullptr, decl_ctxs[1]));

  EXPECT_THAT(ast_parser.GetDeclContextToDIEMapKeys(),
              testing::UnorderedElementsAre(decl_ctxs[0], decl_ctxs[3]));
}

TEST_F(DWARFASTParserClangTests, TestCallingConventionParsing) {
  // Tests parsing DW_AT_calling_convention values.

  // The DWARF below just declares a list of function types with
  // DW_AT_calling_convention on them.
  const char *yamldata = R"(
--- !ELF
FileHeader:
  Class:   ELFCLASS32
  Data:    ELFDATA2LSB
  Type:    ET_EXEC
  Machine: EM_386
DWARF:
  debug_str:
    - func1
    - func2
    - func3
    - func4
    - func5
    - func6
    - func7
    - func8
    - func9
  debug_abbrev:
    - ID:              0
      Table:
        - Code:            0x1
          Tag:             DW_TAG_compile_unit
          Children:        DW_CHILDREN_yes
          Attributes:
            - Attribute:       DW_AT_language
              Form:            DW_FORM_data2
        - Code:            0x2
          Tag:             DW_TAG_subprogram
          Children:        DW_CHILDREN_no
          Attributes:
            - Attribute:       DW_AT_low_pc
              Form:            DW_FORM_addr
            - Attribute:       DW_AT_high_pc
              Form:            DW_FORM_data4
            - Attribute:       DW_AT_name
              Form:            DW_FORM_strp
            - Attribute:       DW_AT_calling_convention
              Form:            DW_FORM_data1
            - Attribute:       DW_AT_external
              Form:            DW_FORM_flag_present
  debug_info:
    - Version:         4
      AddrSize:        4
      Entries:
        - AbbrCode:        0x1
          Values:
            - Value:           0xC
        - AbbrCode:        0x2
          Values:
            - Value:           0x0
            - Value:           0x5
            - Value:           0x00
            - Value:           0xCB
            - Value:           0x1
        - AbbrCode:        0x2
          Values:
            - Value:           0x10
            - Value:           0x5
            - Value:           0x06
            - Value:           0xB3
            - Value:           0x1
        - AbbrCode:        0x2
          Values:
            - Value:           0x20
            - Value:           0x5
            - Value:           0x0C
            - Value:           0xB1
            - Value:           0x1
        - AbbrCode:        0x2
          Values:
            - Value:           0x30
            - Value:           0x5
            - Value:           0x12
            - Value:           0xC0
            - Value:           0x1
        - AbbrCode:        0x2
          Values:
            - Value:           0x40
            - Value:           0x5
            - Value:           0x18
            - Value:           0xB2
            - Value:           0x1
        - AbbrCode:        0x2
          Values:
            - Value:           0x50
            - Value:           0x5
            - Value:           0x1E
            - Value:           0xC1
            - Value:           0x1
        - AbbrCode:        0x2
          Values:
            - Value:           0x60
            - Value:           0x5
            - Value:           0x24
            - Value:           0xC2
            - Value:           0x1
        - AbbrCode:        0x2
          Values:
            - Value:           0x70
            - Value:           0x5
            - Value:           0x2a
            - Value:           0xEE
            - Value:           0x1
        - AbbrCode:        0x2
          Values:
            - Value:           0x80
            - Value:           0x5
            - Value:           0x30
            - Value:           0x01
            - Value:           0x1
        - AbbrCode:        0x0
...
)";
  YAMLModuleTester t(yamldata);

  DWARFUnit *unit = t.GetDwarfUnit();
  ASSERT_NE(unit, nullptr);
  const DWARFDebugInfoEntry *cu_entry = unit->DIE().GetDIE();
  ASSERT_EQ(cu_entry->Tag(), DW_TAG_compile_unit);
  DWARFDIE cu_die(unit, cu_entry);

  auto holder = std::make_unique<clang_utils::TypeSystemClangHolder>("ast");
  auto &ast_ctx = *holder->GetAST();
  DWARFASTParserClangStub ast_parser(ast_ctx);

  std::vector<std::string> found_function_types;
  // The DWARF above is just a list of functions. Parse all of them to
  // extract the function types and their calling convention values.
  for (DWARFDIE func : cu_die.children()) {
    ASSERT_EQ(func.Tag(), DW_TAG_subprogram);
    SymbolContext sc;
    bool new_type = false;
    lldb::TypeSP type = ast_parser.ParseTypeFromDWARF(sc, func, &new_type);
    found_function_types.push_back(
        type->GetForwardCompilerType().GetTypeName().AsCString());
  }

  // Compare the parsed function types against the expected list of types.
  const std::vector<std::string> expected_function_types = {
      "void () __attribute__((regcall))",
      "void () __attribute__((fastcall))",
      "void () __attribute__((stdcall))",
      "void () __attribute__((vectorcall))",
      "void () __attribute__((pascal))",
      "void () __attribute__((ms_abi))",
      "void () __attribute__((sysv_abi))",
      "void ()", // invalid calling convention.
      "void ()", // DW_CC_normal -> no attribute
  };
  ASSERT_EQ(found_function_types, expected_function_types);
}

struct ExtractIntFromFormValueTest : public testing::Test {
  SubsystemRAII<FileSystem, HostInfo> subsystems;
  clang_utils::TypeSystemClangHolder holder;
  TypeSystemClang &ts;

  DWARFASTParserClang parser;
  ExtractIntFromFormValueTest()
      : holder("dummy ASTContext"), ts(*holder.GetAST()), parser(ts) {}

  /// Takes the given integer value, stores it in a DWARFFormValue and then
  /// tries to extract the value back via
  /// DWARFASTParserClang::ExtractIntFromFormValue.
  /// Returns the string representation of the extracted value or the error
  /// that was returned from ExtractIntFromFormValue.
  llvm::Expected<std::string> Extract(clang::QualType qt, uint64_t value) {
    DWARFFormValue form_value;
    form_value.SetUnsigned(value);
    llvm::Expected<llvm::APInt> result =
        parser.ExtractIntFromFormValue(ts.GetType(qt), form_value);
    if (!result)
      return result.takeError();
    llvm::SmallString<16> result_str;
    result->toStringUnsigned(result_str);
    return std::string(result_str.str());
  }

  /// Same as ExtractIntFromFormValueTest::Extract but takes a signed integer
  /// and treats the result as a signed integer.
  llvm::Expected<std::string> ExtractS(clang::QualType qt, int64_t value) {
    DWARFFormValue form_value;
    form_value.SetSigned(value);
    llvm::Expected<llvm::APInt> result =
        parser.ExtractIntFromFormValue(ts.GetType(qt), form_value);
    if (!result)
      return result.takeError();
    llvm::SmallString<16> result_str;
    result->toStringSigned(result_str);
    return std::string(result_str.str());
  }
};

TEST_F(ExtractIntFromFormValueTest, TestBool) {
  using namespace llvm;
  clang::ASTContext &ast = ts.getASTContext();

  EXPECT_THAT_EXPECTED(Extract(ast.BoolTy, 0), HasValue("0"));
  EXPECT_THAT_EXPECTED(Extract(ast.BoolTy, 1), HasValue("1"));
  EXPECT_THAT_EXPECTED(Extract(ast.BoolTy, 2), Failed());
  EXPECT_THAT_EXPECTED(Extract(ast.BoolTy, 3), Failed());
}

TEST_F(ExtractIntFromFormValueTest, TestInt) {
  using namespace llvm;

  clang::ASTContext &ast = ts.getASTContext();

  // Find the min/max values for 'int' on the current host target.
  constexpr int64_t int_max = std::numeric_limits<int>::max();
  constexpr int64_t int_min = std::numeric_limits<int>::min();

  // Check that the bit width of int matches the int width in our type system.
  ASSERT_EQ(sizeof(int) * 8, ast.getIntWidth(ast.IntTy));

  // Check values around int_min.
  EXPECT_THAT_EXPECTED(ExtractS(ast.IntTy, int_min - 2), llvm::Failed());
  EXPECT_THAT_EXPECTED(ExtractS(ast.IntTy, int_min - 1), llvm::Failed());
  EXPECT_THAT_EXPECTED(ExtractS(ast.IntTy, int_min),
                       HasValue(std::to_string(int_min)));
  EXPECT_THAT_EXPECTED(ExtractS(ast.IntTy, int_min + 1),
                       HasValue(std::to_string(int_min + 1)));
  EXPECT_THAT_EXPECTED(ExtractS(ast.IntTy, int_min + 2),
                       HasValue(std::to_string(int_min + 2)));

  // Check values around 0.
  EXPECT_THAT_EXPECTED(ExtractS(ast.IntTy, -128), HasValue("-128"));
  EXPECT_THAT_EXPECTED(ExtractS(ast.IntTy, -10), HasValue("-10"));
  EXPECT_THAT_EXPECTED(ExtractS(ast.IntTy, -1), HasValue("-1"));
  EXPECT_THAT_EXPECTED(ExtractS(ast.IntTy, 0), HasValue("0"));
  EXPECT_THAT_EXPECTED(ExtractS(ast.IntTy, 1), HasValue("1"));
  EXPECT_THAT_EXPECTED(ExtractS(ast.IntTy, 10), HasValue("10"));
  EXPECT_THAT_EXPECTED(ExtractS(ast.IntTy, 128), HasValue("128"));

  // Check values around int_max.
  EXPECT_THAT_EXPECTED(ExtractS(ast.IntTy, int_max - 2),
                       HasValue(std::to_string(int_max - 2)));
  EXPECT_THAT_EXPECTED(ExtractS(ast.IntTy, int_max - 1),
                       HasValue(std::to_string(int_max - 1)));
  EXPECT_THAT_EXPECTED(ExtractS(ast.IntTy, int_max),
                       HasValue(std::to_string(int_max)));
  EXPECT_THAT_EXPECTED(ExtractS(ast.IntTy, int_max + 1), llvm::Failed());
  EXPECT_THAT_EXPECTED(ExtractS(ast.IntTy, int_max + 5), llvm::Failed());

  // Check some values not near an edge case.
  EXPECT_THAT_EXPECTED(ExtractS(ast.IntTy, int_max / 2),
                       HasValue(std::to_string(int_max / 2)));
  EXPECT_THAT_EXPECTED(ExtractS(ast.IntTy, int_min / 2),
                       HasValue(std::to_string(int_min / 2)));
}

TEST_F(ExtractIntFromFormValueTest, TestUnsignedInt) {
  using namespace llvm;

  clang::ASTContext &ast = ts.getASTContext();
  constexpr uint64_t uint_max = std::numeric_limits<uint32_t>::max();

  // Check values around 0.
  EXPECT_THAT_EXPECTED(Extract(ast.UnsignedIntTy, 0), HasValue("0"));
  EXPECT_THAT_EXPECTED(Extract(ast.UnsignedIntTy, 1), HasValue("1"));
  EXPECT_THAT_EXPECTED(Extract(ast.UnsignedIntTy, 1234), HasValue("1234"));

  // Check some values not near an edge case.
  EXPECT_THAT_EXPECTED(Extract(ast.UnsignedIntTy, uint_max / 2),
                       HasValue(std::to_string(uint_max / 2)));

  // Check values around uint_max.
  EXPECT_THAT_EXPECTED(Extract(ast.UnsignedIntTy, uint_max - 2),
                       HasValue(std::to_string(uint_max - 2)));
  EXPECT_THAT_EXPECTED(Extract(ast.UnsignedIntTy, uint_max - 1),
                       HasValue(std::to_string(uint_max - 1)));
  EXPECT_THAT_EXPECTED(Extract(ast.UnsignedIntTy, uint_max),
                       HasValue(std::to_string(uint_max)));
  EXPECT_THAT_EXPECTED(Extract(ast.UnsignedIntTy, uint_max + 1),
                       llvm::Failed());
  EXPECT_THAT_EXPECTED(Extract(ast.UnsignedIntTy, uint_max + 2),
                       llvm::Failed());
}

TEST_F(DWARFASTParserClangTests, TestDefaultTemplateParamParsing) {
  // Tests parsing DW_AT_default_value for template parameters.

  // template <typename T>
  // class foo {};
  //
  // template <template <typename T> class CT = foo>
  // class baz {};
  //
  // template <typename T = char, int i = 3, bool b = true,
  //           typename c = foo<T>>
  // class bar {};
  //
  // int main() {
  //     bar<> br;
  //     baz<> bz;
  //     return 0;
  // }
  //
  // YAML generated on Linux using obj2yaml on the above program
  // compiled with Clang.
  const char *yamldata = R"(
--- !ELF
FileHeader:
  Class:           ELFCLASS64
  Data:            ELFDATA2LSB
  Type:            ET_REL
  Machine:         EM_AARCH64
  SectionHeaderStringTable: .strtab
Sections:
  - Name:            .text
    Type:            SHT_PROGBITS
    Flags:           [ SHF_ALLOC, SHF_EXECINSTR ]
    AddressAlign:    0x4
    Content:         FF4300D1E0031F2AFF0F00B9FF430091C0035FD6
  - Name:            .linker-options
    Type:            SHT_LLVM_LINKER_OPTIONS
    Flags:           [ SHF_EXCLUDE ]
    AddressAlign:    0x1
    Content:         ''
  - Name:            .debug_abbrev
    Type:            SHT_PROGBITS
    AddressAlign:    0x1
    Content:         011101252513050325721710171B25111B120673170000022E01111B1206401803253A0B3B0B49133F190000033400021803253A0B3B0B4913000004240003253E0B0B0B0000050201360B03250B0B3A0B3B0B0000062F00491303251E190000073000491303251E191C0D0000083000491303251E191C0F000009020003253C1900000A8682010003251E19904225000000
  - Name:            .debug_info
    Type:            SHT_PROGBITS
    AddressAlign:    0x1
    Content:         7F00000005000108000000000100210001000000000000000002001400000000000000020014000000016F03000B490000000302910B05000C4D0000000302910A0E000D78000000000404050405050D010009066E000000070749000000080308720000000A0106760000000C000406080104090201090B0505110100050A0F100000
  - Name:            .debug_str_offsets
    Type:            SHT_PROGBITS
    AddressAlign:    0x1
    Content:         4C00000005000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
  - Name:            .comment
    Type:            SHT_PROGBITS
    Flags:           [ SHF_MERGE, SHF_STRINGS ]
    AddressAlign:    0x1
    EntSize:         0x1
    Content:         00636C616E672076657273696F6E2031362E302E30202868747470733A2F2F6769746875622E636F6D2F6C6C766D2F6C6C766D2D70726F6A65637420343764323862376138323638653337616130646537366238353966343530386533646261633663652900
  - Name:            .note.GNU-stack
    Type:            SHT_PROGBITS
    AddressAlign:    0x1
  - Name:            .eh_frame
    Type:            SHT_PROGBITS
    Flags:           [ SHF_ALLOC ]
    AddressAlign:    0x8
    Content:         1000000000000000017A5200017C1E011B0C1F001800000018000000000000001400000000440E104C0E000000000000
  - Name:            .debug_line
    Type:            SHT_PROGBITS
    AddressAlign:    0x1
    Content:         580000000500080037000000010101FB0E0D00010101010000000100000101011F010000000003011F020F051E01000000000019537E33C1D1006B79E3D1C33D6EE6A304000009020000000000000000030A0105050ABD0208000101
  - Name:            .debug_line_str
    Type:            SHT_PROGBITS
    Flags:           [ SHF_MERGE, SHF_STRINGS ]
    AddressAlign:    0x1
    EntSize:         0x1
    Content:         2F686F6D652F6761726465690064656661756C74732E63707000
  - Name:            .rela.debug_info
    Type:            SHT_RELA
    Flags:           [ SHF_INFO_LINK ]
    Link:            .symtab
    AddressAlign:    0x8
    Info:            .debug_info
    Relocations:
      - Offset:          0x8
        Symbol:          .debug_abbrev
        Type:            R_AARCH64_ABS32
      - Offset:          0x11
        Symbol:          .debug_str_offsets
        Type:            R_AARCH64_ABS32
        Addend:          8
      - Offset:          0x15
        Symbol:          .debug_line
        Type:            R_AARCH64_ABS32
      - Offset:          0x1F
        Symbol:          .debug_addr
        Type:            R_AARCH64_ABS32
        Addend:          8
  - Name:            .rela.debug_str_offsets
    Type:            SHT_RELA
    Flags:           [ SHF_INFO_LINK ]
    Link:            .symtab
    AddressAlign:    0x8
    Info:            .debug_str_offsets
    Relocations:
      - Offset:          0x8
        Symbol:          .debug_str
        Type:            R_AARCH64_ABS32
      - Offset:          0xC
        Symbol:          .debug_str
        Type:            R_AARCH64_ABS32
        Addend:          101
      - Offset:          0x10
        Symbol:          .debug_str
        Type:            R_AARCH64_ABS32
        Addend:          114
      - Offset:          0x14
        Symbol:          .debug_str
        Type:            R_AARCH64_ABS32
        Addend:          127
      - Offset:          0x18
        Symbol:          .debug_str
        Type:            R_AARCH64_ABS32
        Addend:          132
      - Offset:          0x1C
        Symbol:          .debug_str
        Type:            R_AARCH64_ABS32
        Addend:          136
      - Offset:          0x20
        Symbol:          .debug_str
        Type:            R_AARCH64_ABS32
        Addend:          139
      - Offset:          0x24
        Symbol:          .debug_str
        Type:            R_AARCH64_ABS32
        Addend:          144
      - Offset:          0x28
        Symbol:          .debug_str
        Type:            R_AARCH64_ABS32
        Addend:          146
      - Offset:          0x2C
        Symbol:          .debug_str
        Type:            R_AARCH64_ABS32
        Addend:          148
      - Offset:          0x30
        Symbol:          .debug_str
        Type:            R_AARCH64_ABS32
        Addend:          153
      - Offset:          0x34
        Symbol:          .debug_str
        Type:            R_AARCH64_ABS32
        Addend:          155
      - Offset:          0x38
        Symbol:          .debug_str
        Type:            R_AARCH64_ABS32
        Addend:          165
      - Offset:          0x3C
        Symbol:          .debug_str
        Type:            R_AARCH64_ABS32
        Addend:          167
      - Offset:          0x40
        Symbol:          .debug_str
        Type:            R_AARCH64_ABS32
        Addend:          198
      - Offset:          0x44
        Symbol:          .debug_str
        Type:            R_AARCH64_ABS32
        Addend:          201
      - Offset:          0x48
        Symbol:          .debug_str
        Type:            R_AARCH64_ABS32
        Addend:          204
      - Offset:          0x4C
        Symbol:          .debug_str
        Type:            R_AARCH64_ABS32
        Addend:          208
  - Name:            .rela.debug_addr
    Type:            SHT_RELA
    Flags:           [ SHF_INFO_LINK ]
    Link:            .symtab
    AddressAlign:    0x8
    Info:            .debug_addr
    Relocations:
      - Offset:          0x8
        Symbol:          .text
        Type:            R_AARCH64_ABS64
  - Name:            .rela.eh_frame
    Type:            SHT_RELA
    Flags:           [ SHF_INFO_LINK ]
    Link:            .symtab
    AddressAlign:    0x8
    Info:            .eh_frame
    Relocations:
      - Offset:          0x1C
        Symbol:          .text
        Type:            R_AARCH64_PREL32
  - Name:            .rela.debug_line
    Type:            SHT_RELA
    Flags:           [ SHF_INFO_LINK ]
    Link:            .symtab
    AddressAlign:    0x8
    Info:            .debug_line
    Relocations:
      - Offset:          0x22
        Symbol:          .debug_line_str
        Type:            R_AARCH64_ABS32
      - Offset:          0x2E
        Symbol:          .debug_line_str
        Type:            R_AARCH64_ABS32
        Addend:          13
      - Offset:          0x48
        Symbol:          .text
        Type:            R_AARCH64_ABS64
  - Name:            .llvm_addrsig
    Type:            SHT_LLVM_ADDRSIG
    Flags:           [ SHF_EXCLUDE ]
    Link:            .symtab
    AddressAlign:    0x1
    Offset:          0x818
    Symbols:         [  ]
  - Type:            SectionHeaderTable
    Sections:
      - Name:            .strtab
      - Name:            .text
      - Name:            .linker-options
      - Name:            .debug_abbrev
      - Name:            .debug_info
      - Name:            .rela.debug_info
      - Name:            .debug_str_offsets
      - Name:            .rela.debug_str_offsets
      - Name:            .debug_str
      - Name:            .debug_addr
      - Name:            .rela.debug_addr
      - Name:            .comment
      - Name:            .note.GNU-stack
      - Name:            .eh_frame
      - Name:            .rela.eh_frame
      - Name:            .debug_line
      - Name:            .rela.debug_line
      - Name:            .debug_line_str
      - Name:            .llvm_addrsig
      - Name:            .symtab
Symbols:
  - Name:            defaults.cpp
    Type:            STT_FILE
    Index:           SHN_ABS
  - Name:            .text
    Type:            STT_SECTION
    Section:         .text
  - Name:            '$x.0'
    Section:         .text
  - Name:            .debug_abbrev
    Type:            STT_SECTION
    Section:         .debug_abbrev
  - Name:            '$d.1'
    Section:         .debug_abbrev
  - Name:            '$d.2'
    Section:         .debug_info
  - Name:            .debug_str_offsets
    Type:            STT_SECTION
    Section:         .debug_str_offsets
  - Name:            '$d.3'
    Section:         .debug_str_offsets
  - Name:            .debug_str
    Type:            STT_SECTION
    Section:         .debug_str
  - Name:            '$d.4'
    Section:         .debug_str
  - Name:            .debug_addr
    Type:            STT_SECTION
    Section:         .debug_addr
  - Name:            '$d.5'
    Section:         .debug_addr
  - Name:            '$d.6'
    Section:         .comment
  - Name:            '$d.7'
    Section:         .eh_frame
  - Name:            .debug_line
    Type:            STT_SECTION
    Section:         .debug_line
  - Name:            '$d.8'
    Section:         .debug_line
  - Name:            .debug_line_str
    Type:            STT_SECTION
    Section:         .debug_line_str
  - Name:            '$d.9'
    Section:         .debug_line_str
  - Name:            main
    Type:            STT_FUNC
    Section:         .text
    Binding:         STB_GLOBAL
    Size:            0x14
DWARF:
  debug_str:
    - 'clang version 16.0.0 (https://github.com/llvm/llvm-project 47d28b7a8268e37aa0de76b859f4508e3dbac6ce)'
    - defaults.cpp
    - '/home/gardei'
    - main
    - int
    - br
    - char
    - T
    - i
    - bool
    - b
    - 'foo<char>'
    - c
    - 'bar<char, 3, true, foo<char> >'
    - bz
    - CT
    - foo
    - 'baz<foo>'
  debug_addr:
    - Length:          0xC
      Version:         0x5
      AddressSize:     0x8
      Entries:
        - {}
...
)";
  YAMLModuleTester t(yamldata);

  DWARFUnit *unit = t.GetDwarfUnit();
  ASSERT_NE(unit, nullptr);
  const DWARFDebugInfoEntry *cu_entry = unit->DIE().GetDIE();
  ASSERT_EQ(cu_entry->Tag(), DW_TAG_compile_unit);
  DWARFDIE cu_die(unit, cu_entry);

  auto holder = std::make_unique<clang_utils::TypeSystemClangHolder>("ast");
  auto &ast_ctx = *holder->GetAST();
  DWARFASTParserClangStub ast_parser(ast_ctx);

  llvm::SmallVector<lldb::TypeSP, 2> types;
  for (DWARFDIE die : cu_die.children()) {
    if (die.Tag() == DW_TAG_class_type) {
      SymbolContext sc;
      bool new_type = false;
      types.push_back(ast_parser.ParseTypeFromDWARF(sc, die, &new_type));
    }
  }

  ASSERT_EQ(types.size(), 3U);

  auto check_decl = [](auto const *decl) {
    clang::ClassTemplateSpecializationDecl const *ctsd =
        llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(decl);
    ASSERT_NE(ctsd, nullptr);
    clang::ClassTemplateDecl const *ctd = ctsd->getSpecializedTemplate();
    ASSERT_NE(ctd, nullptr);

    auto const *params = ctd->getTemplateParameters();
    ASSERT_NE(params, nullptr);

    for (clang::NamedDecl const *param : *params) {
      ASSERT_NE(param, nullptr);
      if (auto *decl = llvm::dyn_cast<clang::TemplateTypeParmDecl>(param)) {
        EXPECT_TRUE(decl->hasDefaultArgument());
      } else if (auto *decl =
                     llvm::dyn_cast<clang::NonTypeTemplateParmDecl>(param)) {
        EXPECT_TRUE(decl->hasDefaultArgument());
      } else if (auto *decl =
                     llvm::dyn_cast<clang::TemplateTemplateParmDecl>(param)) {
        EXPECT_TRUE(decl->hasDefaultArgument());
      } else {
        ASSERT_TRUE(false && "Shouldn't ever get here");
      }
    }
  };

  for (auto const &type_sp : types) {
    ASSERT_NE(type_sp, nullptr);
    auto const *decl = ClangUtil::GetAsTagDecl(type_sp->GetFullCompilerType());
    if (decl->getName() == "bar" || decl->getName() == "baz") {
      check_decl(decl);
    }
  }
}
