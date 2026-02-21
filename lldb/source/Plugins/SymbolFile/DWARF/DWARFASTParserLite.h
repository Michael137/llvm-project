//===-- DWARFASTParserClang.h -----------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_SYMBOLFILE_DWARF_DWARFASTPARSERLITE_H
#define LLDB_SOURCE_PLUGINS_SYMBOLFILE_DWARF_DWARFASTPARSERLITE_H

#include "DWARFASTParser.h"
#include "DWARFDIE.h"

#include "Plugins/TypeSystem/Clang/Lite/TypeSystemLite.h"

struct ParsedDWARFTypeAttributes;

namespace lldb_private {
class DWARFASTParserLite : public plugin::dwarf::DWARFASTParser {
public:
  DWARFASTParserLite(lldb_private::TypeSystemLite &ast) :
    DWARFASTParser(Kind::DWARFASTParserLite),
    m_ast(ast) {}

  ~DWARFASTParserLite() override = default;

  // LLVM RTTI support
  static bool classof(const DWARFASTParser *Parser) {
    return Parser->GetKind() == Kind::DWARFASTParserClang;
  }

  lldb::TypeSP ParseTypeFromDWARF(const SymbolContext &sc,
                                  const plugin::dwarf::DWARFDIE &die,
                                  bool *type_is_new_ptr) override;

  ConstString
  ConstructDemangledNameFromDWARF(const plugin::dwarf::DWARFDIE &die) override {
    return {};
  }

  Function *ParseFunctionFromDWARF(CompileUnit &comp_unit,
                                   const plugin::dwarf::DWARFDIE &die,
                                   AddressRanges ranges) override;

  bool CompleteTypeFromDWARF(const plugin::dwarf::DWARFDIE &die, Type *type,
                             const CompilerType &compiler_type) override {
    return false;
  }

  CompilerDecl
  GetDeclForUIDFromDWARF(const plugin::dwarf::DWARFDIE &die) override {
    return {};
  }

  CompilerDeclContext
  GetDeclContextForUIDFromDWARF(const plugin::dwarf::DWARFDIE &die) override {
    return {};
  }

  CompilerDeclContext GetDeclContextContainingUIDFromDWARF(
      const plugin::dwarf::DWARFDIE &die) override {
    return {};
  }

  void EnsureAllDIEsInDeclContextHaveBeenParsed(
      CompilerDeclContext decl_context) override {}

  std::string GetDIEClassTemplateParams(plugin::dwarf::DWARFDIE die) override {
    return {};
  }

private:
  lldb::TypeSP
  ParseStructureLikeDIE(const SymbolContext &sc,
                        const plugin::dwarf::DWARFDIE &die,
                        ParsedDWARFTypeAttributes &attrs);

  bool ParseChildMembers(
      plugin::dwarf::DWARFDIE parent_die, CompilerType class_clang_type);

  void ParseSingleMember(plugin::dwarf::DWARFDIE die, plugin::dwarf::DWARFDIE parent_die, CompilerType class_clang_type);

  lldb::TypeSP
  ParseTypeModifier(const SymbolContext &sc,
                    const plugin::dwarf::DWARFDIE &die,
                    ParsedDWARFTypeAttributes &attrs);

  // Parent TypeSystemLite.
  TypeSystemLite &m_ast;
};
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_SYMBOLFILE_DWARF_DWARFASTPARSERLITE_H
