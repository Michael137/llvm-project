#include "clang/CrashingContext/ASTContextCrash.h"

int main() {
  // Variable declaration order matters!
  // TODO: copy ASTUnit and ASTContext into ASTContextCrash.h and reduce
  //std::unique_ptr<clang::ASTUnit> FromUnit = nullptr;
  //clang::ASTContext* ast = nullptr;
  
  // TODO: with Crashing::ASTContext we don't crash
  std::unique_ptr<clang::Crashing::ASTUnit> unit = nullptr;
  clang::ASTContext* ast = nullptr;
  //clang::Crashing::ASTContext* ast = nullptr;
  return 0;
}
