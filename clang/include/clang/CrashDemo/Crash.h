#ifndef LLVM_CLANG_CRASHDEMO_CRASH_H
#define LLVM_CLANG_CRASHDEMO_CRASH_H
#include "clang/AST/ExprCXX.h"
//#include "clang/CrashDemoExpr/Expr.h"
namespace clang {
  class Crasher {
    class ImportPathTy {
      llvm::SmallDenseMap<Decl *, int, 32> Aux;
    };
    ImportPathTy ImportPath;

    ExprWithCleanups::CleanupObject
    Import(ExprWithCleanups::CleanupObject From);

    //crashing::ExprWithCleanups::CleanupObject
    //Import(crashing::ExprWithCleanups::CleanupObject From);
  };
} // namespace clang
#endif // _H
