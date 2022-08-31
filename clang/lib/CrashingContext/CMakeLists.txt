set(LLVM_LINK_COMPONENTS
  BinaryFormat
  Core
  FrontendOpenMP
  Support
  )

add_clang_library(clangCrashingContext
  ASTContextCrash.cpp

  LINK_LIBS
  clangAST
  clangBasic
  clangLex
  LLVMCrashingVector

  DEPENDS
  ClangAttrDocTable
  Opcodes
  omp_gen
  )
