#include "clang/AST/Type.h"
#include "clang/CrashDemo/Crash.h"
//#include "clang/Frontend/ASTUnit.h"
#include "clang/CrashDemoASTUnit/ASTUnit.h"

int main() {
  //std::unique_ptr<clang::ASTUnit> ToUnit = nullptr;
  std::unique_ptr<clang::crashing::ASTUnit> ToUnit = nullptr;
  clang::Crasher Importer;
  return 0;
}
