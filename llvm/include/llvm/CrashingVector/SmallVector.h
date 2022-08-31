#ifndef CLANG_CRASHING_CONTEXT_LLVM_VECTOR_H_IN
#include "llvm/Support/type_traits.h"
#include <algorithm>
namespace llvm {
namespace Crashing {

template <class Size_T> class SmallVectorBase { void *BeginX; };

template <typename T, unsigned N>
class SmallVector : public SmallVectorBase<uint32_t> {};
} // namespace Crashing
} // namespace llvm
#endif // _H_IN
