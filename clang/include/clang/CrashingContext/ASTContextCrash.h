#ifndef LLVM_CLANG_CRASHINGCONTEXT_ASTCONTEXTCRASH_H
#include "clang/Serialization/ASTReader.h"
namespace clang {
namespace Crashing {
class ASTUnit {
  SmallVector<StoredDiagnostic, 4> StoredDiagnostics;
};
} // namespace Crashing
} // namespace clang
#endif // _H
