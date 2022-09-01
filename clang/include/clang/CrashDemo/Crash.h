#ifndef LLVM_CLANG_CRASHDEMO_CRASH_H
#include "clang/AST/ASTImportError.h"
#include "clang/AST/ExprCXX.h"
namespace clang {
  class Crasher {
    class ImportPathTy {
      llvm::SmallDenseMap<Decl *, int, 32> Aux;
    };
    ImportPathTy ImportPath;
    template <typename ImportT>
    [[nodiscard]] llvm::Error importInto(ImportT &To, const ImportT &From) {}
    llvm::Expected<ExprWithCleanups::CleanupObject>
    Import(ExprWithCleanups::CleanupObject From);
  };
} // namespace clang
#endif // _H
