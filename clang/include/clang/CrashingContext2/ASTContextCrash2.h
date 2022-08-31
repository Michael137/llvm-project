#ifndef LLVM_CLANG_CRASHINGCONTEXT2_ASTCONTEXTCRASH2_H
#include "clang/Serialization/ASTReader.h"
#include "clang/AST/Type.h"
namespace clang {
namespace Crashing2 {
class ASTContextCrash2 {
  mutable llvm::SmallVector<clang::Type *, 0> Types;
};
} // namespace Crashing2
} // namespace clang
#endif // _H
