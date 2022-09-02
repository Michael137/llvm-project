#ifndef LLVM_CLANG_CRASHDEMO_ASTUNIT_H
#include "clang/Frontend/CompilerInvocation.h"
#include <utility>
namespace clang {
namespace crashing {
class ASTUnit {
  IntrusiveRefCntPtr<ASTContext>          Ctx;
  FileSystemOptions FileSystemOpts;
  std::vector<Decl*> TopLevelDecls;
  std::string OriginalSourceFile;
};
}
} // namespace clang
#endif // LLVM_CLANG_FRONTEND_ASTUNIT_H
