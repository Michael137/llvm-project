#include "TypeSystemLite.h"

#include "Plugins/SymbolFile/DWARF/DWARFASTParserLite.h"

#include <memory>

#include "llvm/Support/ErrorHandling.h"
#include "llvm/TargetParser/Triple.h"
#include "llvm/ADT/StringRef.h"

#include "lldb/Target/Target.h"
#include "lldb/Core/PluginManager.h"
#include "lldb/Core/Module.h"

using namespace lldb_private;
using namespace lldb_private::plugin::dwarf;

LLDB_PLUGIN_DEFINE(TypeSystemLite)

//TypeSystemLite::TypeSystemLite(llvm::StringRef name, llvm::Triple triple) {
//  //m_display_name = name.str();
//  //if (!triple.str().empty())
//  //  SetTargetTriple(triple.str());
//  //// The caller didn't pass an ASTContext so create a new one for this
//  //// TypeSystemClang.
//  //CreateASTContext();
//
//  //LogCreation();
//}

TypeSystemLite::TypeSystemLite() {}
TypeSystemLite::~TypeSystemLite() {}

lldb::TypeSystemSP TypeSystemLite::CreateInstance(lldb::LanguageType language,
                                                   lldb_private::Module *module,
                                                   Target *target) {
  if (!SupportsLanguageStatic(language))
    return lldb::TypeSystemSP();
  ArchSpec arch;
  if (module)
    arch = module->GetArchitecture();
  else if (target)
    arch = target->GetArchitecture();

  if (!arch.IsValid())
    return lldb::TypeSystemSP();

  llvm::Triple triple = arch.GetTriple();
  // LLVM wants this to be set to iOS or MacOSX; if we're working on
  // a bare-boards type image, change the triple for llvm's benefit.
  if (triple.getVendor() == llvm::Triple::Apple &&
      triple.getOS() == llvm::Triple::UnknownOS) {
    if (triple.getArch() == llvm::Triple::arm ||
        triple.getArch() == llvm::Triple::aarch64 ||
        triple.getArch() == llvm::Triple::aarch64_32 ||
        triple.getArch() == llvm::Triple::thumb) {
      triple.setOS(llvm::Triple::IOS);
    } else {
      triple.setOS(llvm::Triple::MacOSX);
    }
  }

  if (module) {
    std::string ast_name =
        "ASTContext for '" + module->GetFileSpec().GetPath() + "'";
    //return std::make_shared<TypeSystemLite>(ast_name, triple);
    return std::make_shared<TypeSystemLite>();
  }

  return lldb::TypeSystemSP();
}

LanguageSet TypeSystemLite::GetSupportedLanguagesForTypes() {
  LanguageSet languages;
#ifdef LITE
  languages.Insert(lldb::eLanguageTypeC89);
  languages.Insert(lldb::eLanguageTypeC);
  languages.Insert(lldb::eLanguageTypeC11);
  languages.Insert(lldb::eLanguageTypeC_plus_plus);
  languages.Insert(lldb::eLanguageTypeC99);
  languages.Insert(lldb::eLanguageTypeObjC);
  languages.Insert(lldb::eLanguageTypeObjC_plus_plus);
  languages.Insert(lldb::eLanguageTypeC_plus_plus_03);
  languages.Insert(lldb::eLanguageTypeC_plus_plus_11);
  languages.Insert(lldb::eLanguageTypeC11);
  languages.Insert(lldb::eLanguageTypeC_plus_plus_14);
  languages.Insert(lldb::eLanguageTypeC_plus_plus_17);
  languages.Insert(lldb::eLanguageTypeC_plus_plus_20);
#endif
  return languages;
}

LanguageSet TypeSystemLite::GetSupportedLanguagesForExpressions() {
  LanguageSet languages;
#ifdef LITE
  languages.Insert(lldb::eLanguageTypeC_plus_plus);
  languages.Insert(lldb::eLanguageTypeObjC_plus_plus);
  languages.Insert(lldb::eLanguageTypeC_plus_plus_03);
  languages.Insert(lldb::eLanguageTypeC_plus_plus_11);
  languages.Insert(lldb::eLanguageTypeC_plus_plus_14);
  languages.Insert(lldb::eLanguageTypeC_plus_plus_17);
  languages.Insert(lldb::eLanguageTypeC_plus_plus_20);
#endif
  return languages;
}

void TypeSystemLite::Initialize() {
  PluginManager::RegisterPlugin(
      GetPluginNameStatic(), "light-weight AST context plug-in", CreateInstance,
      GetSupportedLanguagesForTypes(), GetSupportedLanguagesForExpressions());
}

void TypeSystemLite::Terminate() {
  PluginManager::UnregisterPlugin(CreateInstance);
}

DWARFASTParser *TypeSystemLite::GetDWARFParser() {
  if (!m_dwarf_ast_parser_up)
    m_dwarf_ast_parser_up = std::make_unique<DWARFASTParserLite>(*this);
  return m_dwarf_ast_parser_up.get();
}

char TypeSystemLite::ID;

CompilerType TypeSystemLite::GetBuiltinTypeForDWARFEncodingAndBitSize(
        llvm::StringRef name, uint32_t encoding, uint32_t bit_size) {

  auto *type = new LiteType;

  if (name == "int") {
      type->name = ConstString(name);
      type->byte_size = 4;
      return CompilerType(weak_from_this(), type);
  }

  llvm_unreachable("TODO");
}
