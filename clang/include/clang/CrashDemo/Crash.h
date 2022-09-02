#ifndef LLVM_CLANG_CRASHDEMO_CRASH_H
#include "clang/AST/ExprCXX.h"
namespace clang {
  class Crasher {
    class ImportPathTy {
      llvm::SmallDenseMap<Decl *, int, 32> Aux;
    };
    ImportPathTy ImportPath;

    ExprWithCleanups::CleanupObject
    Import(ExprWithCleanups::CleanupObject From);

  };
} // namespace clang
#endif // _H
