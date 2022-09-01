#include "clang/CrashDemo/Crash.h"
#include "clang/Frontend/ASTUnit.h"

int main() {
  std::unique_ptr<clang::ASTUnit> ToUnit = nullptr;
  std::unique_ptr<clang::ASTUnit> FromUnit = nullptr;
  clang::Crasher Importer;
  return 0;
}
