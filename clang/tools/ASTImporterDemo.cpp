#include "clang/AST/ASTImporter.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Basic/SourceLocation.h"

using namespace clang;
using namespace tooling;
using namespace ast_matchers;

template <typename Node, typename Matcher>
Node *getFirstDecl(Matcher M, const std::unique_ptr<ASTUnit> &Unit) {
  auto MB = M.bind("bindStr"); // Bind the to-be-matched node to a string key.
  auto MatchRes = match(MB, Unit->getASTContext());
  // We should have at least one match.
  assert(MatchRes.size() >= 1);
  // Get the first matched and bound node.
  Node *Result =
      const_cast<Node *>(MatchRes[0].template getNodeAs<Node>("bindStr"));
  assert(Result);
  return Result;
}

int main() {
  std::unique_ptr<ASTUnit> ToUnit = buildASTFromCode(
      "", "to.cc");
  std::unique_ptr<ASTUnit> FromUnit = buildASTFromCode(
      R"(
      class MyClass {
        int m1;
        int m2;
      };
      )",
      "from.cc");
  auto Matcher = cxxRecordDecl(hasName("MyClass"));
  auto *From = getFirstDecl<CXXRecordDecl>(Matcher, FromUnit);

  ASTImporter Importer(ToUnit->getASTContext(), ToUnit->getFileManager(),
                       FromUnit->getASTContext(), FromUnit->getFileManager(),
                       /*MinimalImport=*/true);
  llvm::Expected<Decl *> ImportedOrErr = Importer.Import(From);
  if (!ImportedOrErr) {
    llvm::Error Err = ImportedOrErr.takeError();
    llvm::errs() << "ERROR: " << Err << "\n";
    consumeError(std::move(Err));
    return 1;
  }
  Decl *Imported = *ImportedOrErr;
  Imported->getTranslationUnitDecl()->dump();

  if (llvm::Error Err = Importer.ImportDefinition(From)) {
    llvm::errs() << "ERROR: " << Err << "\n";
    consumeError(std::move(Err));
    return 1;
  }
  llvm::errs() << "Imported definition.\n";
  Imported->getTranslationUnitDecl()->dump();

  SourceLocation sloc;

  return sloc.isValid();
}
