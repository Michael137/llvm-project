#ifndef LLVM_CLANG_CRASHINGCONTEXT2_ASTCONTEXTCRASH2_H
#include "llvm/CrashingVector/SmallVector.h"

namespace clang {
class ClassInMod1 {
  //mutable llvm::SmallVector<clang::Type *, 0> Types;
  mutable llvm::ClassInMod3<int> VecInMod1;
};
} // namespace clang
#endif // _H
