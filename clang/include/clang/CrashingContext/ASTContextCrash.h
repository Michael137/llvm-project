#ifndef LLVM_CLANG_CRASHINGCONTEXT_ASTCONTEXTCRASH_H
#define LLVM_CLANG_CRASHINGCONTEXT_ASTCONTEXTCRASH_H
#include "llvm/CrashingVector/SmallVector.h"
namespace clang {
class ClassInMod2 {
  //SmallVector<StoredDiagnostic, 1> StoredDiagnostics;
    llvm::ClassInMod3<float> VecInMod2; // NOTE: the first template
                                        // parameter has to be different
                                        // from the one in ContextCrash2
                                        // for the ItaniumRecordLayoutBuilder
                                        // assertion to trigger
};
} // namespace clang
#endif // _H
