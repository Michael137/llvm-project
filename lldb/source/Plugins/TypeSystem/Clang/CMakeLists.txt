lldb_tablegen(TypeSystemClangProperties.inc -gen-lldb-property-defs
  SOURCE TypeSystemClangProperties.td
  TARGET TypeSystemClangPropertiesGen)

lldb_tablegen(TypeSystemClangPropertiesEnum.inc -gen-lldb-property-enum-defs
  SOURCE TypeSystemClangProperties.td
  TARGET TypeSystemClangPropertiesEnumGen)

add_lldb_library(lldbPluginTypeSystemClang PLUGIN
  TypeSystemClang.cpp

  LINK_LIBS
    lldbCore
    lldbSymbol
    lldbTarget
    lldbUtility
    lldbPluginExpressionParserClang
    lldbPluginSymbolFileDWARF
    lldbPluginSymbolFilePDB
    lldbPluginObjCRuntime
  CLANG_LIBS
    clangAST
    clangBasic
    clangFrontend
    clangSema
  LINK_COMPONENTS
    Support
)

add_dependencies(lldbPluginTypeSystemClang
  TypeSystemClangPropertiesGen
  TypeSystemClangPropertiesEnumGen)
