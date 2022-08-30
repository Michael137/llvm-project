#ifndef LLVM_CLANG_AST_ASTIMPORTER_H
#define LLVM_CLANG_AST_ASTIMPORTER_H

#include "clang/AST/ASTImportError.h"
#include "clang/AST/ASTImporterSharedState.h"
#include "clang/AST/DeclBase.h"
#include "clang/AST/DeclarationName.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/NestedNameSpecifier.h"
#include "clang/AST/TemplateName.h"
#include "clang/AST/Type.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/IdentifierTable.h"
#include "clang/Basic/LLVM.h"
#include "clang/Basic/SourceLocation.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/SmallVector.h"
#include <utility>

namespace clang {

class ASTContext;
class ASTImporterSharedState;
class Attr;
class CXXBaseSpecifier;
class CXXCtorInitializer;
class Decl;
class DeclContext;
class Expr;
class FileManager;
class NamedDecl;
class Stmt;
class TagDecl;
class TranslationUnitDecl;
class TypeSourceInfo;

  class ASTImporterCrash {
    friend class ASTNodeImporter;
  public:
    enum class ODRHandlingType { Conservative, Liberal };

    class ImportPathTy {
    public:
      using VecTy = llvm::SmallVector<Decl *, 32>;
    private:
      // All nodes of the path.
      VecTy Nodes;
      // Auxiliary container to be able to answer "Do we have a cycle ending
      // at last element?" as fast as possible.
      // We count each Decl's occurrence over the path.
      llvm::SmallDenseMap<Decl *, int, 32> Aux;
    };

  private:
    /// The path which we go through during the import of a given AST node.
    ImportPathTy ImportPath;

    /// The contexts we're importing to and from.
    ASTContext &ToContext, &FromContext;

    /// The file managers we're importing to and from.
    FileManager &ToFileManager, &FromFileManager;

    /// Whether to perform a minimal import.
    bool Minimal;
  public:

    ASTImporterCrash(ASTContext &ToContext, FileManager &ToFileManager,
                ASTContext &FromContext, FileManager &FromFileManager,
                bool MinimalImport,
                std::shared_ptr<ASTImporterSharedState> SharedState = nullptr);
        
    virtual ~ASTImporterCrash();
  };
} // namespace clang

#endif // LLVM_CLANG_AST_ASTIMPORTER_H
