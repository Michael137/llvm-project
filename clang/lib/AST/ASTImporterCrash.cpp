#include "clang/AST/ASTImporterCrash.h"

namespace clang {
ASTImporterCrash::ASTImporterCrash(
    ASTContext &ToContext, FileManager &ToFileManager)
    : ToContext(ToContext),
      ToFileManager(ToFileManager)
      {}
} // namespace clang
