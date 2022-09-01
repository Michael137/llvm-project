#ifndef CLANG_CRASHING_CONTEXT_LLVM_VECTOR_H_IN
#include "llvm/Support/type_traits.h"
#include <algorithm>
namespace llvm {
template <class Size_T> class ClassInMod3Base { void *BeginX; };

template <typename T>
class ClassInMod3 : public ClassInMod3Base<uint32_t> {};
} // namespace llvm
#endif // _H_IN
