add_lldb_library(lldbPluginCPPRuntime
  CPPLanguageRuntime.cpp
  CPlusPlusCallableThreadPlan.cpp

  LINK_LIBS
    lldbCore
    lldbSymbol
    lldbTarget
)

add_subdirectory(ItaniumABI)
#add_subdirectory(MicrosoftABI)
