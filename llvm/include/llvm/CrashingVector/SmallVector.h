#ifndef CLANG_CRASHING_CONTEXT_LLVM_VECTOR_H_IN
#include "llvm/Support/type_traits.h"
#include <algorithm>
namespace llvm {
namespace Crashing {
template <typename T> class ArrayRef;
template <class Iterator>
using EnableIfConvertibleToInputIterator = std::enable_if_t<std::is_convertible<
    typename std::iterator_traits<Iterator>::iterator_category,
    std::input_iterator_tag>::value>;
template <class Size_T> class SmallVectorBase {
protected:
  void *BeginX;
  Size_T Size = 0, Capacity;
  SmallVectorBase(void *FirstEl, size_t TotalCapacity)
      : BeginX(FirstEl), Capacity(TotalCapacity) {}
};
template <class T>
using SmallVectorSizeType =
    std::conditional_t<sizeof(T) < 4 && sizeof(void *) >= 8, uint64_t,
                       uint32_t>;
template <class T, typename = void> struct SmallVectorAlignmentAndSize {
};
template <typename T, typename = void>
class SmallVectorTemplateCommon
    : public SmallVectorBase<SmallVectorSizeType<T>> {
  using Base = SmallVectorBase<SmallVectorSizeType<T>>;
  void *getFirstEl() const {
  }
protected:
  SmallVectorTemplateCommon(size_t Size) : Base(getFirstEl(), Size) {}
  bool isReferenceToRange(const void *V, const void *First, const void *Last) const {
  }
  bool isRangeInStorage(const void *First, const void *Last) const {
  }
  bool isSafeToReferenceAfterResize(const void *Elt, size_t NewSize) {
  }
  void assertSafeToReferenceAfterResize(const void *Elt, size_t NewSize) {
    assert(isSafeToReferenceAfterResize(Elt, NewSize) &&
           "that invalidates it");
  }
  template <
      class ItTy,
      std::enable_if_t<!std::is_same<std::remove_const_t<ItTy>, T *>::value,
                       bool> = false>
  void assertSafeToAddRange(ItTy, ItTy) {}
  template <class U>
  static const T *reserveForParamAndGetAddressImpl(U *This, const T &Elt,
                                                   size_t N) {
    if (!U::TakesParamByValue) {
      if (LLVM_UNLIKELY(This->isReferenceToStorage(&Elt))) {
      }
    }
  }
  using size_type = size_t;
  using iterator = T *;
  using reference = T &;
  reference back() {
  }
};
template <typename T, bool = (std::is_trivially_copy_constructible<T>::value) &&
                             std::is_trivially_destructible<T>::value>
class SmallVectorTemplateBase : public SmallVectorTemplateCommon<T> {
protected:
  using ValueParamT = const T &;
  SmallVectorTemplateBase(size_t Size) : SmallVectorTemplateCommon<T>(Size) {}
  static void destroy_range(T *S, T *E) {
    while (S != E) {
    }
  }
  template<typename It1, typename It2>
  static void uninitialized_copy(It1 I, It1 E, It2 Dest) {
  }
  T *mallocForGrow(size_t MinSize, size_t &NewCapacity) {
    return static_cast<T *>(
        SmallVectorBase<SmallVectorSizeType<T>>::mallocForGrow(
            MinSize, sizeof(T), NewCapacity));
  }
  void takeAllocationForGrow(T *NewElts, size_t NewCapacity);
  void growAndAssign(size_t NumElts, const T &Elt) {
  }
  void push_back(const T &Elt) {
  }
};
template <typename T, bool TriviallyCopyable>
void SmallVectorTemplateBase<T, TriviallyCopyable>::takeAllocationForGrow(
    T *NewElts, size_t NewCapacity) {
}
template <typename T>
class SmallVectorTemplateBase<T, true> : public SmallVectorTemplateCommon<T> {
protected:
  static constexpr bool TakesParamByValue = sizeof(T) <= 2 * sizeof(void *);
  using ValueParamT =
      typename std::conditional<TakesParamByValue, T, const T &>::type;
  SmallVectorTemplateBase(size_t Size) : SmallVectorTemplateCommon<T>(Size) {}
  template<typename It1, typename It2>
  static void uninitialized_move(It1 I, It1 E, It2 Dest) {
  }
  const T *reserveForParamAndGetAddress(const T &Elt, size_t N = 1) {
  }
};
template <typename T>
class SmallVectorImpl : public SmallVectorTemplateBase<T> {
  using SuperClass = SmallVectorTemplateBase<T>;
public:
  using iterator = typename SuperClass::iterator;
  using size_type = typename SuperClass::size_type;
  using ValueParamT = typename SuperClass::ValueParamT;
  explicit SmallVectorImpl(unsigned N)
      : SmallVectorTemplateBase<T>(N) {}
  void assignRemote(SmallVectorImpl &&RHS) {
  }
  template <bool ForOverwrite> void resizeImpl(size_type N) {
    if (N < this->size()) {
    }
  }
  void resize(size_type N, ValueParamT NV) {
  }
  void swap(SmallVectorImpl &RHS);
  template <typename ItTy, typename = EnableIfConvertibleToInputIterator<ItTy>>
  void append(ItTy in_start, ItTy in_end) {
  }
  iterator insert(iterator I, size_type NumToInsert, ValueParamT Elt) {
    if (I == this->end()) {  // Important special case for empty vector.
    }
    if (size_t(this->end()-I) >= NumToInsert) {
    }
  }
  template <typename ItTy, typename = EnableIfConvertibleToInputIterator<ItTy>>
  iterator insert(iterator I, ItTy From, ItTy To) {
    size_t NumToInsert = std::distance(From, To);
    if (size_t(this->end()-I) >= NumToInsert) {
    }
  }
  void insert(iterator I, std::initializer_list<T> IL) {
  }
};
template <typename T>
void SmallVectorImpl<T>::swap(SmallVectorImpl<T> &RHS) {
  if (!this->isSmall() && !RHS.isSmall()) {
  }
  size_t RHSSize = RHS.size();
  size_t CurSize = this->size();
  if (CurSize >= RHSSize) {
  }
}
template <typename T, unsigned N>
struct SmallVectorStorage {
};
template <typename T, unsigned N> class LLVM_GSL_OWNER SmallVector;
template <typename T> struct CalculateSmallVectorDefaultInlinedElements {
  static constexpr size_t kPreferredSmallVectorSizeof = 64;
  static_assert(
      "sure you really want that much inline storage.");
  static constexpr size_t PreferredInlineBytes =
      kPreferredSmallVectorSizeof - sizeof(SmallVector<T, 0>);
  static constexpr size_t NumElementsThatFit = PreferredInlineBytes / sizeof(T);
  static constexpr size_t value =
      NumElementsThatFit == 0 ? 1 : NumElementsThatFit;
};
template <typename T,
          unsigned N = CalculateSmallVectorDefaultInlinedElements<T>::value>
class LLVM_GSL_OWNER SmallVector : public SmallVectorImpl<T>,
                                   SmallVectorStorage<T, N> {
public:
  SmallVector() : SmallVectorImpl<T>(N) {}
  explicit SmallVector(size_t Size, const T &Value = T())
    : SmallVectorImpl<T>(N) {
  }
  template <typename U,
            typename = std::enable_if_t<std::is_convertible<U, T>::value>>
  explicit SmallVector(ArrayRef<U> A) : SmallVectorImpl<T>(N) {
  }
  SmallVector(const SmallVector &RHS) : SmallVectorImpl<T>(N) {
      SmallVectorImpl<T>::operator=(::std::move(RHS));
  }
  SmallVector &operator=(SmallVector &&RHS) {
    if (N) {
    }
  }
};
}
}
#endif // _H_IN
