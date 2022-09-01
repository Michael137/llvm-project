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
    ASTContext &ToContext, &FromContext;
    FileManager &ToFileManager, &FromFileManager;
    bool Minimal;
    ODRHandlingType ODRHandling;
    llvm::DenseMap<Decl *, Decl *> ImportedDecls;
  public:
    Crasher(ASTContext &ToContext, FileManager &ToFileManager,
                         ASTContext &FromContext, FileManager &FromFileManager,
                         bool MinimalImport,
                         std::shared_ptr<ASTImporterSharedState> SharedState = nullptr)
    : SharedState(SharedState), ToContext(ToContext), FromContext(FromContext),
      ToFileManager(ToFileManager), FromFileManager(FromFileManager),
      Minimal(MinimalImport), ODRHandling(ODRHandlingType::Conservative) {}

    //virtual ~Crasher() = default;
    template <typename ImportT>
    [[nodiscard]] llvm::Error importInto(ImportT &To, const ImportT &From) {
    }
    llvm::Expected<ExprWithCleanups::CleanupObject>
    Import(ExprWithCleanups::CleanupObject From);
  };
} // namespace clang
#endif // _H
