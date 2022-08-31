#include "clang/AST/ASTImporter.h"
#include "clang/Tooling/Tooling.h"
//class Bar {
//  clang::ASTContext &FromContext;
//
//public:
//  Bar(
//      clang::ASTContext &FromContext
//      )
//      :
//        FromContext(FromContext)
//        {}
//  ~Bar() = default;
//};
//int main() {
//  std::unique_ptr<clang::ASTUnit> FromUnit =
//      clang::tooling::buildASTFromCode("", "from.cc");
//  Bar Importer(FromUnit->getASTContext());
//  return 0;
//}

int main() {
  std::unique_ptr<clang::ASTUnit> FromUnit =
      clang::tooling::buildASTFromCode("", "from.cc"); // <<< we don't crash if FromUnit is not in scope
  auto& FromContext = FromUnit->getASTContext();
  return 0;
}

