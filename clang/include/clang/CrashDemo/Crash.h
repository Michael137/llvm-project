#ifndef LLVM_CLANG_CRASHDEMO_CRASH_H
#include "clang/AST/ASTImportError.h"
#include "clang/AST/ExprCXX.h"
namespace clang {
class ASTImporterSharedState;
class FileManager;
  class Crasher {
    enum class ODRHandlingType { Conservative, Liberal };
    class ImportPathTy {
      llvm::SmallDenseMap<Decl *, int, 32> Aux;
    };
    std::shared_ptr<ASTImporterSharedState> SharedState = nullptr;
    ImportPathTy ImportPath;
    ASTContext *ToContext;
    ASTContext *FromContext;
    FileManager *ToFileManager;
    FileManager *FromFileManager;
    bool Minimal;
    ODRHandlingType ODRHandling;
    llvm::DenseMap<Decl *, Decl *> ImportedDecls;
  public:
    template <typename ImportT>
    [[nodiscard]] llvm::Error importInto(ImportT &To, const ImportT &From) {}
    llvm::Expected<ExprWithCleanups::CleanupObject>
    Import(ExprWithCleanups::CleanupObject From);
  };
} // namespace clang
#endif // _H
