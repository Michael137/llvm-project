#include "clang/CrashingContext/ASTContextCrash.h"
#include "clang/CrashingContext2/ASTContextCrash2.h"

int main() {
  // Non-reduced
  // Module 1: Clang_FrontEnd
  //std::unique_ptr<clang::ASTUnit> FromUnit = nullptr;
  // Module 2: Clang_AST
  //clang::ASTContext* ast = nullptr;
  
  // Reduced
  // Module 1: Clang_ContextCrashing
  clang::ClassInMod1 fromMod1;
  // Module 2: Clang_ContextCrashing2
  clang::ClassInMod2 fromMod2;
  return 0;
}
