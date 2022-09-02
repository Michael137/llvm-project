set(LLVM_LINK_COMPONENTS
  BinaryFormat
  Core
  FrontendOpenMP
  Support
  )

add_clang_library(clangCrashDemoExpr
  Expr.cpp

  LINK_LIBS
  clangAST
  clangBasic
  clangLex

  DEPENDS
  ClangAttrDocTable
  Opcodes
  omp_gen
  )
