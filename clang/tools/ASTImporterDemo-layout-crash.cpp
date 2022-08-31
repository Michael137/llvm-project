#include "clang/AST/ASTImporterCrash.h"
#include "clang/Tooling/Tooling.h"

int main() {
  std::unique_ptr<clang::ASTUnit> ToUnit =
      clang::tooling::buildASTFromCode("", "to.cc");
  clang::ASTImporterCrash Importer(ToUnit->getASTContext(), ToUnit->getFileManager());
  return 0;
}
