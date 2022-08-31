#include "clang/CrashingContext/ASTContextCrash.h"
#include "clang/CrashingContext2/ASTContextCrash2.h"

int main() {
  // Non-reduced
  // Module 1: Clang_FrontEnd
  //std::unique_ptr<clang::ASTUnit> FromUnit = nullptr;
  // Module 1: Clang_AST
  //clang::ASTContext* ast = nullptr;
  
  // Reduced
  // Module 1: Clang_ContextCrashing
  clang::Crashing::ASTUnit unit;
  // Module 1: Clang_ContextCrashing2
  clang::Crashing2::ASTContextCrash2 ast;
  return 0;
}
