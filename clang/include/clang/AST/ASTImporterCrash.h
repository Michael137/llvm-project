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
class FileManager;

class ASTImporterCrash {
public:
  ASTImporterCrash(ASTContext &ToContext, FileManager &ToFileManager);
  ~ASTImporterCrash() = default;

private:
  llvm::SmallVector<Decl *, 32> ImportPath;
  ASTContext &ToContext;
  FileManager &ToFileManager;
};
} // namespace clang

#endif // LLVM_CLANG_AST_ASTIMPORTER_H
