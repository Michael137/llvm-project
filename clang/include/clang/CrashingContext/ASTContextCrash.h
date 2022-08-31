#ifndef LLVM_CLANG_CRASHINGCONTEXT_ASTCONTEXTCRASH_H
#include "clang/Serialization/ASTReader.h"
#include "llvm/CrashingVector/SmallVector.h"
namespace clang {
namespace Crashing {
class ASTUnit {
  //SmallVector<StoredDiagnostic, 4> StoredDiagnostics; // Uncomment to crash in LayoutFields
    llvm::Crashing::SmallVector<StoredDiagnostic, 4> StoredDiagnostics; // Uncomment to crash in LayoutNonVirtualBases
};
} // namespace Crashing
} // namespace clang
#endif // _H
