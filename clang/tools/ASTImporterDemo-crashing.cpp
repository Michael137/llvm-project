#include "clang/AST/ASTImporter.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Basic/SourceLocation.h"

int main() {
  std::unique_ptr<clang::ASTUnit> ToUnit = clang::tooling::buildASTFromCode(
      "", "to.cc");
  std::unique_ptr<clang::ASTUnit> FromUnit = clang::tooling::buildASTFromCode(
      R"(
      class MyClass {
        int m1;
        int m2;
      };
      )",
      "from.cc");
  //auto Matcher = cxxRecordDecl(hasName("MyClass"));
  //auto *From = getFirstDecl<CXXRecordDecl>(Matcher, FromUnit);

  clang::ASTImporter Importer(ToUnit->getASTContext(), ToUnit->getFileManager(),
                       FromUnit->getASTContext(), FromUnit->getFileManager(),
                       /*MinimalImport=*/true);
  return 0;
}
