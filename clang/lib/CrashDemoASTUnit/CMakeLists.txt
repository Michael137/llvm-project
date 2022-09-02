set(LLVM_LINK_COMPONENTS
  BinaryFormat
  Core
  FrontendOpenMP
  Support
  )

add_clang_library(clangCrashDemoASTUnit
  ASTUnit.cpp

  LINK_LIBS
  clangAST
  clangBasic
  clangLex

  DEPENDS
  ClangAttrDocTable
  Opcodes
  omp_gen
  )
