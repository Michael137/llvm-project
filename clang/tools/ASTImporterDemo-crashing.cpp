#include "clang/CrashDemo/Crash.h"
#include "clang/AST/ASTImporter.h"
#include "clang/Tooling/Tooling.h"

int main() {
  std::unique_ptr<clang::ASTUnit> ToUnit =
      clang::tooling::buildASTFromCode("", "to.cc");
  std::unique_ptr<clang::ASTUnit> FromUnit =
      clang::tooling::buildASTFromCode("",
                                       "from.cc");
  clang::Crasher Importer(ToUnit->getASTContext(), ToUnit->getFileManager(),
                              FromUnit->getASTContext(),
                              FromUnit->getFileManager(),
                              /*MinimalImport=*/true);
  return 0;
}
