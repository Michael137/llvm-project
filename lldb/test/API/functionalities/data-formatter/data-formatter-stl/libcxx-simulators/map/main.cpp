#include <__compare/synth_three_way.h>
#include <__functional/is_transparent.h>
#include <__memory/allocator_traits.h>
#include <__memory/swap_allocator.h>
#include <__type_traits/is_swappable.h>
#include <__utility/swap.h>

#include <__tree>
#include <algorithm>
#include <iterator>
#include <type_traits>

namespace std {
namespace __lldb {
template <class, class, class, class> class map;

template <class _Tp, class _Compare, class _Allocator> class __tree;
template <class _Tp, class _NodePtr, class _DiffType> class __tree_iterator;
template <class _Tp, class _ConstNodePtr, class _DiffType>
class __tree_const_iterator;

template <class _Pointer> class __tree_end_node;
template <class _VoidPtr> class __tree_node_base;
template <class _Tp, class _VoidPtr> class __tree_node;

template <class _Key, class _Value> struct __value_type;

template <class _Allocator> class __map_node_destructor;
template <class _TreeIterator> class __map_iterator;
template <class _TreeIterator> class __map_const_iterator;

// node traits

template <class _Tp> struct __is_tree_value_type_imp : false_type {};

template <class _Key, class _Value>
struct __is_tree_value_type_imp<__value_type<_Key, _Value>> : true_type {};

template <class... _Args> struct __is_tree_value_type : false_type {};

template <class _One>
struct __is_tree_value_type<_One>
    : __is_tree_value_type_imp<__remove_cvref_t<_One>> {};

template <class _Tp> struct __tree_key_value_types {
  typedef _Tp key_type;
  typedef _Tp __node_value_type;
  typedef _Tp __container_value_type;
  static const bool __is_map = false;

  static key_type const &__get_key(_Tp const &__v) { return __v; }
  static __container_value_type const &
  __get_value(__node_value_type const &__v) {
    return __v;
  }
  static __container_value_type *__get_ptr(__node_value_type &__n) {
    return std::addressof(__n);
  }
  static __container_value_type &&__move(__node_value_type &__v) {
    return std::move(__v);
  }
};

template <class _Key, class _Tp>
struct __tree_key_value_types<__value_type<_Key, _Tp>> {
  typedef _Key key_type;
  typedef _Tp mapped_type;
  typedef __value_type<_Key, _Tp> __node_value_type;
  typedef pair<const _Key, _Tp> __container_value_type;
  typedef __container_value_type __map_value_type;
  static const bool __is_map = true;

  static __container_value_type *__get_ptr(__node_value_type &__n) {
    return std::addressof(__n.__get_value());
  }
};

template <class _VoidPtr> struct __tree_node_base_types {
  typedef _VoidPtr __void_pointer;

  typedef __tree_node_base<__void_pointer> __node_base_type;
  typedef __rebind_pointer_t<_VoidPtr, __node_base_type> __node_base_pointer;

  typedef __tree_end_node<__node_base_pointer> __end_node_type;
  typedef __rebind_pointer_t<_VoidPtr, __end_node_type> __end_node_pointer;
#if defined(_LIBCPP_ABI_TREE_REMOVE_NODE_POINTER_UB)
  typedef __end_node_pointer __parent_pointer;
#else
  typedef __conditional_t<is_pointer<__end_node_pointer>::value,
                          __end_node_pointer, __node_base_pointer>
      __parent_pointer;
#endif

private:
  static_assert(
      is_same<typename pointer_traits<_VoidPtr>::element_type, void>::value,
      "_VoidPtr does not point to unqualified void type");
};

template <class _Tp, class _AllocPtr,
          class _KVTypes = __tree_key_value_types<_Tp>,
          bool = _KVTypes::__is_map>
struct __tree_map_pointer_types {};

template <class _Tp, class _AllocPtr, class _KVTypes>
struct __tree_map_pointer_types<_Tp, _AllocPtr, _KVTypes, true> {
  typedef typename _KVTypes::__map_value_type _Mv;
  typedef __rebind_pointer_t<_AllocPtr, _Mv> __map_value_type_pointer;
  typedef __rebind_pointer_t<_AllocPtr, const _Mv>
      __const_map_value_type_pointer;
};

template <class _NodePtr,
          class _NodeT = typename pointer_traits<_NodePtr>::element_type>
struct __tree_node_types;

template <class _NodePtr, class _Tp, class _VoidPtr>
struct __tree_node_types<_NodePtr, __tree_node<_Tp, _VoidPtr>>
    : public __tree_node_base_types<_VoidPtr>,
      __tree_key_value_types<_Tp>,
      __tree_map_pointer_types<_Tp, _VoidPtr> {
  typedef __tree_node_base_types<_VoidPtr> __base;
  typedef __tree_key_value_types<_Tp> __key_base;
  typedef __tree_map_pointer_types<_Tp, _VoidPtr> __map_pointer_base;

public:
  typedef typename pointer_traits<_NodePtr>::element_type __node_type;
  typedef _NodePtr __node_pointer;

  typedef _Tp __node_value_type;
  typedef __rebind_pointer_t<_VoidPtr, __node_value_type>
      __node_value_type_pointer;
  typedef __rebind_pointer_t<_VoidPtr, const __node_value_type>
      __const_node_value_type_pointer;
#if defined(_LIBCPP_ABI_TREE_REMOVE_NODE_POINTER_UB)
  typedef typename __base::__end_node_pointer __iter_pointer;
#else
  typedef __conditional_t<is_pointer<__node_pointer>::value,
                          typename __base::__end_node_pointer, __node_pointer>
      __iter_pointer;
#endif

private:
  static_assert(!is_const<__node_type>::value,
                "_NodePtr should never be a pointer to const");
  static_assert(
      is_same<__rebind_pointer_t<_VoidPtr, __node_type>, _NodePtr>::value,
      "_VoidPtr does not rebind to _NodePtr.");
};

template <class _ValueTp, class _VoidPtr> struct __make_tree_node_types {
  typedef __rebind_pointer_t<_VoidPtr, __tree_node<_ValueTp, _VoidPtr>>
      _NodePtr;
  typedef __tree_node_types<_NodePtr> type;
};

// node

template <class _Pointer> class __tree_end_node {
public:
  typedef _Pointer pointer;
  pointer __left_;

  __tree_end_node() noexcept : __left_() {}
};

template <class _VoidPtr>
class __attribute__((__standalone_debug__)) __tree_node_base
    : public __tree_node_base_types<_VoidPtr>::__end_node_type {
  typedef __tree_node_base_types<_VoidPtr> _NodeBaseTypes;

public:
  typedef typename _NodeBaseTypes::__node_base_pointer pointer;
  typedef typename _NodeBaseTypes::__parent_pointer __parent_pointer;

  pointer __right_;
  __parent_pointer __parent_;
  bool __is_black_;

  pointer __parent_unsafe() const { return static_cast<pointer>(__parent_); }

  void __set_parent(pointer __p) {
    __parent_ = static_cast<__parent_pointer>(__p);
  }

  ~__tree_node_base() = delete;
  __tree_node_base(__tree_node_base const &) = delete;
  __tree_node_base &operator=(__tree_node_base const &) = delete;
};

template <class _Tp, class _VoidPtr>
class __attribute__((__standalone_debug__)) __tree_node
    : public __tree_node_base<_VoidPtr> {
public:
  typedef _Tp __node_value_type;

  __node_value_type __value_;

  _Tp &__get_value() { return __value_; }

  ~__tree_node() = delete;
  __tree_node(__tree_node const &) = delete;
  __tree_node &operator=(__tree_node const &) = delete;
};

template <class _Allocator> class __tree_node_destructor {
  typedef _Allocator allocator_type;
  typedef allocator_traits<allocator_type> __alloc_traits;

public:
  typedef typename __alloc_traits::pointer pointer;

private:
  typedef __tree_node_types<pointer> _NodeTypes;
  allocator_type &__na_;

public:
  bool __value_constructed;

  __tree_node_destructor(const __tree_node_destructor &) = default;
  __tree_node_destructor &operator=(const __tree_node_destructor &) = delete;

  explicit __tree_node_destructor(allocator_type &__na,
                                  bool __val = false) noexcept
      : __na_(__na), __value_constructed(__val) {}

  void operator()(pointer __p) noexcept {
    if (__value_constructed)
      __alloc_traits::destroy(__na_, _NodeTypes::__get_ptr(__p->__value_));
    if (__p)
      __alloc_traits::deallocate(__na_, __p, 1);
  }

  template <class> friend class __map_node_destructor;
};

template <class _Tp, class _NodePtr, class _DiffType> class __tree_iterator {
  typedef __tree_node_types<_NodePtr> _NodeTypes;
  typedef _NodePtr __node_pointer;
  typedef typename _NodeTypes::__node_base_pointer __node_base_pointer;
  typedef typename _NodeTypes::__end_node_pointer __end_node_pointer;
  typedef typename _NodeTypes::__iter_pointer __iter_pointer;
  typedef pointer_traits<__node_pointer> __pointer_traits;

  __iter_pointer __ptr_;

public:
  typedef bidirectional_iterator_tag iterator_category;
  typedef _Tp value_type;
  typedef _DiffType difference_type;
  typedef value_type &reference;
  typedef typename _NodeTypes::__node_value_type_pointer pointer;

  __tree_iterator() noexcept : __ptr_(nullptr) {}

  reference operator*() const { return __get_np()->__value_; }
  pointer operator->() const {
    return pointer_traits<pointer>::pointer_to(__get_np()->__value_);
  }

  __tree_iterator &operator++() {
    __ptr_ =
        static_cast<__iter_pointer>(std::__tree_next_iter<__end_node_pointer>(
            static_cast<__node_base_pointer>(__ptr_)));
    return *this;
  }
  __tree_iterator operator++(int) {
    __tree_iterator __t(*this);
    ++(*this);
    return __t;
  }

  __tree_iterator &operator--() {
    __ptr_ =
        static_cast<__iter_pointer>(std::__tree_prev_iter<__node_base_pointer>(
            static_cast<__end_node_pointer>(__ptr_)));
    return *this;
  }
  __tree_iterator operator--(int) {
    __tree_iterator __t(*this);
    --(*this);
    return __t;
  }

  friend bool operator==(const __tree_iterator &__x,
                         const __tree_iterator &__y) {
    return __x.__ptr_ == __y.__ptr_;
  }
  friend bool operator!=(const __tree_iterator &__x,
                         const __tree_iterator &__y) {
    return !(__x == __y);
  }

private:
  explicit __tree_iterator(__node_pointer __p) noexcept : __ptr_(__p) {}
  explicit __tree_iterator(__end_node_pointer __p) noexcept : __ptr_(__p) {}
  __node_pointer __get_np() const {
    return static_cast<__node_pointer>(__ptr_);
  }
  template <class, class, class> friend class __tree;
  template <class, class, class> friend class __tree_const_iterator;
  template <class> friend class __map_iterator;
  template <class, class, class, class> friend class map;
};

template <class _Tp, class _NodePtr, class _DiffType>
class __tree_const_iterator {
  typedef __tree_node_types<_NodePtr> _NodeTypes;
  typedef typename _NodeTypes::__node_pointer __node_pointer;
  typedef typename _NodeTypes::__node_base_pointer __node_base_pointer;
  typedef typename _NodeTypes::__end_node_pointer __end_node_pointer;
  typedef typename _NodeTypes::__iter_pointer __iter_pointer;
  typedef pointer_traits<__node_pointer> __pointer_traits;

  __iter_pointer __ptr_;

public:
  typedef bidirectional_iterator_tag iterator_category;
  typedef _Tp value_type;
  typedef _DiffType difference_type;
  typedef const value_type &reference;
  typedef typename _NodeTypes::__const_node_value_type_pointer pointer;

  __tree_const_iterator() noexcept : __ptr_(nullptr) {}

private:
  typedef __tree_iterator<value_type, __node_pointer, difference_type>
      __non_const_iterator;

public:
  __tree_const_iterator(__non_const_iterator __p) noexcept
      : __ptr_(__p.__ptr_) {}

  reference operator*() const { return __get_np()->__value_; }
  pointer operator->() const {
    return pointer_traits<pointer>::pointer_to(__get_np()->__value_);
  }

  __tree_const_iterator &operator++() {
    __ptr_ =
        static_cast<__iter_pointer>(std::__tree_next_iter<__end_node_pointer>(
            static_cast<__node_base_pointer>(__ptr_)));
    return *this;
  }

  __tree_const_iterator operator++(int) {
    __tree_const_iterator __t(*this);
    ++(*this);
    return __t;
  }

  __tree_const_iterator &operator--() {
    __ptr_ =
        static_cast<__iter_pointer>(std::__tree_prev_iter<__node_base_pointer>(
            static_cast<__end_node_pointer>(__ptr_)));
    return *this;
  }

  __tree_const_iterator operator--(int) {
    __tree_const_iterator __t(*this);
    --(*this);
    return __t;
  }

  friend bool operator==(const __tree_const_iterator &__x,
                         const __tree_const_iterator &__y) {
    return __x.__ptr_ == __y.__ptr_;
  }
  friend bool operator!=(const __tree_const_iterator &__x,
                         const __tree_const_iterator &__y) {
    return !(__x == __y);
  }

private:
  explicit __tree_const_iterator(__node_pointer __p) noexcept : __ptr_(__p) {}
  explicit __tree_const_iterator(__end_node_pointer __p) noexcept
      : __ptr_(__p) {}
  __node_pointer __get_np() const {
    return static_cast<__node_pointer>(__ptr_);
  }

  template <class, class, class> friend class __tree;
  template <class, class, class, class> friend class map;
  template <class> friend class __map_const_iterator;
};

template <class _Tp, class _Compare>
#ifndef _LIBCPP_CXX03_LANG
_LIBCPP_DIAGNOSE_WARNING(
    !__invokable<_Compare const &, _Tp const &, _Tp const &>::value,
    "the specified comparator type does not provide a viable const call "
    "operator")
#endif
int __diagnose_non_const_comparator();

template <class _Tp, class _Compare, class _Allocator> class __tree {
public:
  typedef _Tp value_type;
  typedef _Compare value_compare;
  typedef _Allocator allocator_type;

private:
  typedef allocator_traits<allocator_type> __alloc_traits;
  typedef typename __make_tree_node_types<
      value_type, typename __alloc_traits::void_pointer>::type _NodeTypes;
  typedef typename _NodeTypes::key_type key_type;

public:
  typedef typename _NodeTypes::__node_value_type __node_value_type;
  typedef typename _NodeTypes::__container_value_type __container_value_type;

  typedef typename __alloc_traits::pointer pointer;
  typedef typename __alloc_traits::const_pointer const_pointer;
  typedef typename __alloc_traits::size_type size_type;
  typedef typename __alloc_traits::difference_type difference_type;

public:
  typedef typename _NodeTypes::__void_pointer __void_pointer;

  typedef typename _NodeTypes::__node_type __node;
  typedef typename _NodeTypes::__node_pointer __node_pointer;

  typedef typename _NodeTypes::__node_base_type __node_base;
  typedef typename _NodeTypes::__node_base_pointer __node_base_pointer;

  typedef typename _NodeTypes::__end_node_type __end_node_t;
  typedef typename _NodeTypes::__end_node_pointer __end_node_ptr;

  typedef typename _NodeTypes::__parent_pointer __parent_pointer;
  typedef typename _NodeTypes::__iter_pointer __iter_pointer;

  typedef __rebind_alloc<__alloc_traits, __node> __node_allocator;
  typedef allocator_traits<__node_allocator> __node_traits;

private:
  // check for sane allocator pointer rebinding semantics. Rebinding the
  // allocator for a new pointer type should be exactly the same as rebinding
  // the pointer using 'pointer_traits'.
  static_assert(is_same<__node_pointer, typename __node_traits::pointer>::value,
                "Allocator does not rebind pointers in a sane manner.");
  typedef __rebind_alloc<__node_traits, __node_base> __node_base_allocator;
  typedef allocator_traits<__node_base_allocator> __node_base_traits;
  static_assert(
      is_same<__node_base_pointer, typename __node_base_traits::pointer>::value,
      "Allocator does not rebind pointers in a sane manner.");

private:
  __iter_pointer __begin_node_;
  __compressed_pair<__end_node_t, __node_allocator> __pair1_;
  __compressed_pair<size_type, value_compare> __pair3_;

public:
  __iter_pointer __end_node() noexcept {
    return static_cast<__iter_pointer>(
        pointer_traits<__end_node_ptr>::pointer_to(__pair1_.first()));
  }
  __iter_pointer __end_node() const noexcept {
    return static_cast<__iter_pointer>(
        pointer_traits<__end_node_ptr>::pointer_to(
            const_cast<__end_node_t &>(__pair1_.first())));
  }
  __node_allocator &__node_alloc() noexcept { return __pair1_.second(); }

private:
  const __node_allocator &__node_alloc() const noexcept {
    return __pair1_.second();
  }
  __iter_pointer &__begin_node() noexcept { return __begin_node_; }
  const __iter_pointer &__begin_node() const noexcept { return __begin_node_; }

public:
  allocator_type __alloc() const noexcept {
    return allocator_type(__node_alloc());
  }

private:
  size_type &size() noexcept { return __pair3_.first(); }

public:
  const size_type &size() const noexcept { return __pair3_.first(); }
  value_compare &value_comp() noexcept { return __pair3_.second(); }
  const value_compare &value_comp() const noexcept { return __pair3_.second(); }

public:
  __node_pointer __root() const noexcept {
    return static_cast<__node_pointer>(__end_node()->__left_);
  }

  __node_base_pointer *__root_ptr() const noexcept {
    return std::addressof(__end_node()->__left_);
  }

  typedef __tree_iterator<value_type, __node_pointer, difference_type> iterator;
  typedef __tree_const_iterator<value_type, __node_pointer, difference_type>
      const_iterator;

  explicit __tree(const value_compare &__comp) noexcept(
      is_nothrow_default_constructible<__node_allocator>::value &&
      is_nothrow_copy_constructible<value_compare>::value);
  explicit __tree(const allocator_type &__a);
  __tree(const value_compare &__comp, const allocator_type &__a);
  __tree(const __tree &__t);
  __tree &operator=(const __tree &__t);
  template <class _ForwardIterator>
  void __assign_unique(_ForwardIterator __first, _ForwardIterator __last);
  template <class _InputIterator>
  void __assign_multi(_InputIterator __first, _InputIterator __last);
  __tree(__tree &&__t) noexcept(
      is_nothrow_move_constructible<__node_allocator>::value &&
      is_nothrow_move_constructible<value_compare>::value);
  __tree(__tree &&__t, const allocator_type &__a);
  __tree &operator=(__tree &&__t) noexcept(
      __node_traits::propagate_on_container_move_assignment::value &&
      is_nothrow_move_assignable<value_compare>::value &&
      is_nothrow_move_assignable<__node_allocator>::value);
  ~__tree();

  iterator begin() noexcept { return iterator(__begin_node()); }
  const_iterator begin() const noexcept {
    return const_iterator(__begin_node());
  }
  iterator end() noexcept { return iterator(__end_node()); }
  const_iterator end() const noexcept { return const_iterator(__end_node()); }

  size_type max_size() const noexcept {
    return std::min<size_type>(__node_traits::max_size(__node_alloc()),
                               numeric_limits<difference_type>::max());
  }

  template <class _Key, class... _Args>
  pair<iterator, bool> __emplace_unique_key_args(_Key const &,
                                                 _Args &&...__args);

  iterator __remove_node_pointer(__node_pointer) noexcept;

  iterator erase(const_iterator __p);

  void __insert_node_at(__parent_pointer __parent, __node_base_pointer &__child,
                        __node_base_pointer __new_node) noexcept;

  typedef __tree_node_destructor<__node_allocator> _Dp;
  typedef unique_ptr<__node, _Dp> __node_holder;

private:
  // FIXME: Make this function const qualified. Unfortunately doing so
  // breaks existing code which uses non-const callable comparators.
  template <class _Key>
  __node_base_pointer &__find_equal(__parent_pointer &__parent,
                                    const _Key &__v);
  template <class _Key>
  __node_base_pointer &__find_equal(__parent_pointer &__parent,
                                    const _Key &__v) const {
    return const_cast<__tree *>(this)->__find_equal(__parent, __v);
  }
  template <class _Key>
  __node_base_pointer &
  __find_equal(const_iterator __hint, __parent_pointer &__parent,
               __node_base_pointer &__dummy, const _Key &__v);

  template <class... _Args> __node_holder __construct_node(_Args &&...__args);

  // TODO: Make this
  _LIBCPP_HIDDEN void destroy(__node_pointer __nd) noexcept;

  template <class, class, class, class> friend class map;
};

template <class _Tp, class _Compare, class _Allocator>
__tree<_Tp, _Compare, _Allocator>::__tree(const value_compare &__comp,
                                          const allocator_type &__a)
    : __begin_node_(__iter_pointer()),
      __pair1_(__default_init_tag(), __node_allocator(__a)),
      __pair3_(0, __comp) {
  __begin_node() = __end_node();
}

template <class _Tp, class _Compare, class _Allocator>
__tree<_Tp, _Compare, _Allocator>::~__tree() {
  static_assert(is_copy_constructible<value_compare>::value,
                "Comparator must be copy-constructible.");
  destroy(__root());
}

template <class _Tp, class _Compare, class _Allocator>
void __tree<_Tp, _Compare, _Allocator>::destroy(__node_pointer __nd) noexcept {
  if (__nd != nullptr) {
    destroy(static_cast<__node_pointer>(__nd->__left_));
    destroy(static_cast<__node_pointer>(__nd->__right_));
    __node_allocator &__na = __node_alloc();
    __node_traits::destroy(__na, _NodeTypes::__get_ptr(__nd->__value_));
    __node_traits::deallocate(__na, __nd, 1);
  }
}

// Find place to insert if __v doesn't exist
// Set __parent to parent of null leaf
// Return reference to null leaf
// If __v exists, set parent to node of __v and return reference to node of __v
template <class _Tp, class _Compare, class _Allocator>
template <class _Key>
typename __tree<_Tp, _Compare, _Allocator>::__node_base_pointer &
__tree<_Tp, _Compare, _Allocator>::__find_equal(__parent_pointer &__parent,
                                                const _Key &__v) {
  __node_pointer __nd = __root();
  __node_base_pointer *__nd_ptr = __root_ptr();
  if (__nd != nullptr) {
    while (true) {
      if (value_comp()(__v, __nd->__value_)) {
        if (__nd->__left_ != nullptr) {
          __nd_ptr = std::addressof(__nd->__left_);
          __nd = static_cast<__node_pointer>(__nd->__left_);
        } else {
          __parent = static_cast<__parent_pointer>(__nd);
          return __parent->__left_;
        }
      } else if (value_comp()(__nd->__value_, __v)) {
        if (__nd->__right_ != nullptr) {
          __nd_ptr = std::addressof(__nd->__right_);
          __nd = static_cast<__node_pointer>(__nd->__right_);
        } else {
          __parent = static_cast<__parent_pointer>(__nd);
          return __nd->__right_;
        }
      } else {
        __parent = static_cast<__parent_pointer>(__nd);
        return *__nd_ptr;
      }
    }
  }
  __parent = static_cast<__parent_pointer>(__end_node());
  return __parent->__left_;
}

// Find place to insert if __v doesn't exist
// First check prior to __hint.
// Next check after __hint.
// Next do O(log N) search.
// Set __parent to parent of null leaf
// Return reference to null leaf
// If __v exists, set parent to node of __v and return reference to node of __v
template <class _Tp, class _Compare, class _Allocator>
template <class _Key>
typename __tree<_Tp, _Compare, _Allocator>::__node_base_pointer &
__tree<_Tp, _Compare, _Allocator>::__find_equal(const_iterator __hint,
                                                __parent_pointer &__parent,
                                                __node_base_pointer &__dummy,
                                                const _Key &__v) {
  if (__hint == end() || value_comp()(__v, *__hint)) // check before
  {
    // __v < *__hint
    const_iterator __prior = __hint;
    if (__prior == begin() || value_comp()(*--__prior, __v)) {
      // *prev(__hint) < __v < *__hint
      if (__hint.__ptr_->__left_ == nullptr) {
        __parent = static_cast<__parent_pointer>(__hint.__ptr_);
        return __parent->__left_;
      } else {
        __parent = static_cast<__parent_pointer>(__prior.__ptr_);
        return static_cast<__node_base_pointer>(__prior.__ptr_)->__right_;
      }
    }
    // __v <= *prev(__hint)
    return __find_equal(__parent, __v);
  } else if (value_comp()(*__hint, __v)) // check after
  {
    // *__hint < __v
    const_iterator __next = std::next(__hint);
    if (__next == end() || value_comp()(__v, *__next)) {
      // *__hint < __v < *std::next(__hint)
      if (__hint.__get_np()->__right_ == nullptr) {
        __parent = static_cast<__parent_pointer>(__hint.__ptr_);
        return static_cast<__node_base_pointer>(__hint.__ptr_)->__right_;
      } else {
        __parent = static_cast<__parent_pointer>(__next.__ptr_);
        return __parent->__left_;
      }
    }
    // *next(__hint) <= __v
    return __find_equal(__parent, __v);
  }
  // else __v == *__hint
  __parent = static_cast<__parent_pointer>(__hint.__ptr_);
  __dummy = static_cast<__node_base_pointer>(__hint.__ptr_);
  return __dummy;
}

template <class _Tp, class _Compare, class _Allocator>
void __tree<_Tp, _Compare, _Allocator>::__insert_node_at(
    __parent_pointer __parent, __node_base_pointer &__child,
    __node_base_pointer __new_node) noexcept {
  __new_node->__left_ = nullptr;
  __new_node->__right_ = nullptr;
  __new_node->__parent_ = __parent;
  // __new_node->__is_black_ is initialized in __tree_balance_after_insert
  __child = __new_node;
  if (__begin_node()->__left_ != nullptr)
    __begin_node() = static_cast<__iter_pointer>(__begin_node()->__left_);
  std::__tree_balance_after_insert(__end_node()->__left_, __child);
  ++size();
}

template <class _Tp, class _Compare, class _Allocator>
template <class _Key, class... _Args>
pair<typename __tree<_Tp, _Compare, _Allocator>::iterator, bool>
__tree<_Tp, _Compare, _Allocator>::__emplace_unique_key_args(
    _Key const &__k, _Args &&...__args) {
  __parent_pointer __parent;
  __node_base_pointer &__child = __find_equal(__parent, __k);
  __node_pointer __r = static_cast<__node_pointer>(__child);
  bool __inserted = false;
  if (__child == nullptr) {
    __node_holder __h = __construct_node(std::forward<_Args>(__args)...);
    __insert_node_at(__parent, __child,
                     static_cast<__node_base_pointer>(__h.get()));
    __r = __h.release();
    __inserted = true;
  }
  return pair<iterator, bool>(iterator(__r), __inserted);
}

template <class _Tp, class _Compare, class _Allocator>
template <class... _Args>
typename __tree<_Tp, _Compare, _Allocator>::__node_holder
__tree<_Tp, _Compare, _Allocator>::__construct_node(_Args &&...__args) {
  static_assert(!__is_tree_value_type<_Args...>::value,
                "Cannot construct from __value_type");
  __node_allocator &__na = __node_alloc();
  __node_holder __h(__node_traits::allocate(__na, 1), _Dp(__na));
  __node_traits::construct(__na, _NodeTypes::__get_ptr(__h->__value_),
                           std::forward<_Args>(__args)...);
  __h.get_deleter().__value_constructed = true;
  return __h;
}

template <class _Tp, class _Compare, class _Allocator>
typename __tree<_Tp, _Compare, _Allocator>::iterator
__tree<_Tp, _Compare, _Allocator>::__remove_node_pointer(
    __node_pointer __ptr) noexcept {
  iterator __r(__ptr);
  ++__r;
  if (__begin_node() == __ptr)
    __begin_node() = __r.__ptr_;
  --size();
  std::__tree_remove(__end_node()->__left_,
                     static_cast<__node_base_pointer>(__ptr));
  return __r;
}

template <class _Tp, class _Compare, class _Allocator>
typename __tree<_Tp, _Compare, _Allocator>::iterator
__tree<_Tp, _Compare, _Allocator>::erase(const_iterator __p) {
  __node_pointer __np = __p.__get_np();
  iterator __r = __remove_node_pointer(__np);
  __node_allocator &__na = __node_alloc();
  __node_traits::destroy(
      __na, _NodeTypes::__get_ptr(const_cast<__node_value_type &>(*__p)));
  __node_traits::deallocate(__na, __np, 1);
  return __r;
}

template <class _Key, class _CP, class _Compare,
          bool =
              is_empty<_Compare>::value && !__libcpp_is_final<_Compare>::value>
class __map_value_compare : private _Compare {
public:
  __map_value_compare() noexcept(
      is_nothrow_default_constructible<_Compare>::value)
      : _Compare() {}
  __map_value_compare(_Compare __c) noexcept(
      is_nothrow_copy_constructible<_Compare>::value)
      : _Compare(__c) {}
  const _Compare &key_comp() const noexcept { return *this; }
  bool operator()(const _CP &__x, const _CP &__y) const {
    return static_cast<const _Compare &>(*this)(__x.__get_value().first,
                                                __y.__get_value().first);
  }
  bool operator()(const _CP &__x, const _Key &__y) const {
    return static_cast<const _Compare &>(*this)(__x.__get_value().first, __y);
  }
  bool operator()(const _Key &__x, const _CP &__y) const {
    return static_cast<const _Compare &>(*this)(__x, __y.__get_value().first);
  }

  template <typename _K2>
  bool operator()(const _K2 &__x, const _CP &__y) const {
    return static_cast<const _Compare &>(*this)(__x, __y.__get_value().first);
  }

  template <typename _K2>
  bool operator()(const _CP &__x, const _K2 &__y) const {
    return static_cast<const _Compare &>(*this)(__x.__get_value().first, __y);
  }
};

template <class _Key, class _CP, class _Compare>
class __map_value_compare<_Key, _CP, _Compare, false> {
  _Compare __comp_;

public:
  __map_value_compare() noexcept(
      is_nothrow_default_constructible<_Compare>::value)
      : __comp_() {}
  __map_value_compare(_Compare __c) noexcept(
      is_nothrow_copy_constructible<_Compare>::value)
      : __comp_(__c) {}
  const _Compare &key_comp() const noexcept { return __comp_; }

  bool operator()(const _CP &__x, const _CP &__y) const {
    return __comp_(__x.__get_value().first, __y.__get_value().first);
  }
  bool operator()(const _CP &__x, const _Key &__y) const {
    return __comp_(__x.__get_value().first, __y);
  }
  bool operator()(const _Key &__x, const _CP &__y) const {
    return __comp_(__x, __y.__get_value().first);
  }

  template <typename _K2>
  bool operator()(const _K2 &__x, const _CP &__y) const {
    return __comp_(__x, __y.__get_value().first);
  }

  template <typename _K2>
  bool operator()(const _CP &__x, const _K2 &__y) const {
    return __comp_(__x.__get_value().first, __y);
  }
};

template <class _Allocator> class __map_node_destructor {
  typedef _Allocator allocator_type;
  typedef allocator_traits<allocator_type> __alloc_traits;

public:
  typedef typename __alloc_traits::pointer pointer;

private:
  allocator_type &__na_;

public:
  bool __first_constructed;
  bool __second_constructed;

  explicit __map_node_destructor(allocator_type &__na) noexcept
      : __na_(__na), __first_constructed(false), __second_constructed(false) {}

  __map_node_destructor(__tree_node_destructor<allocator_type> &&__x) noexcept
      : __na_(__x.__na_), __first_constructed(__x.__value_constructed),
        __second_constructed(__x.__value_constructed) {
    __x.__value_constructed = false;
  }

  __map_node_destructor &operator=(const __map_node_destructor &) = delete;

  void operator()(pointer __p) noexcept {
    if (__second_constructed)
      __alloc_traits::destroy(
          __na_, std::addressof(__p->__value_.__get_value().second));
    if (__first_constructed)
      __alloc_traits::destroy(
          __na_, std::addressof(__p->__value_.__get_value().first));
    if (__p)
      __alloc_traits::deallocate(__na_, __p, 1);
  }
};

template <class _Key, class _Tp, class _Compare, class _Allocator> class map;

template <class _Key, class _Tp>
struct __attribute__((__standalone_debug__)) __value_type {
  typedef _Key key_type;
  typedef _Tp mapped_type;
  typedef pair<const key_type, mapped_type> value_type;
  typedef pair<key_type &, mapped_type &> __nc_ref_pair_type;
  typedef pair<key_type &&, mapped_type &&> __nc_rref_pair_type;

private:
  value_type __cc_;

public:
  value_type &__get_value() { return __cc_; }

  const value_type &__get_value() const { return __cc_; }

  __nc_ref_pair_type __ref() {
    value_type &__v = __get_value();
    return __nc_ref_pair_type(const_cast<key_type &>(__v.first), __v.second);
  }

  __nc_rref_pair_type __move() {
    value_type &__v = __get_value();
    return __nc_rref_pair_type(std::move(const_cast<key_type &>(__v.first)),
                               std::move(__v.second));
  }

  __value_type &operator=(const __value_type &__v) {
    __ref() = __v.__get_value();
    return *this;
  }

  __value_type &operator=(__value_type &&__v) {
    __ref() = __v.__move();
    return *this;
  }

  template <class _ValueTp,
            std::__enable_if_t<
                std::__is_same_uncvref<_ValueTp, value_type>::value, int> = 0>
  __value_type &operator=(_ValueTp &&__v) {
    __ref() = std::forward<_ValueTp>(__v);
    return *this;
  }

  __value_type() = delete;
  ~__value_type() = delete;
  __value_type(const __value_type &) = delete;
  __value_type(__value_type &&) = delete;
};

template <class _Tp> struct __extract_key_value_types;

template <class _Key, class _Tp>
struct __extract_key_value_types<__value_type<_Key, _Tp>> {
  typedef _Key const __key_type;
  typedef _Tp __mapped_type;
};

template <class _TreeIterator> class __map_iterator {
  typedef typename _TreeIterator::_NodeTypes _NodeTypes;
  typedef typename _TreeIterator::__pointer_traits __pointer_traits;

  _TreeIterator __i_;

public:
  typedef bidirectional_iterator_tag iterator_category;
  typedef typename _NodeTypes::__map_value_type value_type;
  typedef typename _TreeIterator::difference_type difference_type;
  typedef value_type &reference;
  typedef typename _NodeTypes::__map_value_type_pointer pointer;

  __map_iterator() noexcept {}

  __map_iterator(_TreeIterator __i) noexcept : __i_(__i) {}

  reference operator*() const { return __i_->__get_value(); }
  pointer operator->() const {
    return pointer_traits<pointer>::pointer_to(__i_->__get_value());
  }

  __map_iterator &operator++() {
    ++__i_;
    return *this;
  }
  __map_iterator operator++(int) {
    __map_iterator __t(*this);
    ++(*this);
    return __t;
  }

  __map_iterator &operator--() {
    --__i_;
    return *this;
  }
  __map_iterator operator--(int) {
    __map_iterator __t(*this);
    --(*this);
    return __t;
  }

  friend bool operator==(const __map_iterator &__x, const __map_iterator &__y) {
    return __x.__i_ == __y.__i_;
  }
  friend bool operator!=(const __map_iterator &__x, const __map_iterator &__y) {
    return __x.__i_ != __y.__i_;
  }

  template <class, class, class, class> friend class map;
  template <class> friend class __map_const_iterator;
};

template <class _TreeIterator> class __map_const_iterator {
  typedef typename _TreeIterator::_NodeTypes _NodeTypes;
  typedef typename _TreeIterator::__pointer_traits __pointer_traits;

  _TreeIterator __i_;

public:
  typedef bidirectional_iterator_tag iterator_category;
  typedef typename _NodeTypes::__map_value_type value_type;
  typedef typename _TreeIterator::difference_type difference_type;
  typedef const value_type &reference;
  typedef typename _NodeTypes::__const_map_value_type_pointer pointer;

  __map_const_iterator() noexcept {}

  __map_const_iterator(_TreeIterator __i) noexcept : __i_(__i) {}

  __map_const_iterator(
      __map_iterator<typename _TreeIterator::__non_const_iterator> __i) noexcept
      : __i_(__i.__i_) {}

  reference operator*() const { return __i_->__get_value(); }
  pointer operator->() const {
    return pointer_traits<pointer>::pointer_to(__i_->__get_value());
  }

  __map_const_iterator &operator++() {
    ++__i_;
    return *this;
  }
  __map_const_iterator operator++(int) {
    __map_const_iterator __t(*this);
    ++(*this);
    return __t;
  }

  __map_const_iterator &operator--() {
    --__i_;
    return *this;
  }
  __map_const_iterator operator--(int) {
    __map_const_iterator __t(*this);
    --(*this);
    return __t;
  }

  friend bool operator==(const __map_const_iterator &__x,
                         const __map_const_iterator &__y) {
    return __x.__i_ == __y.__i_;
  }
  friend bool operator!=(const __map_const_iterator &__x,
                         const __map_const_iterator &__y) {
    return __x.__i_ != __y.__i_;
  }

  template <class, class, class, class> friend class map;
  template <class, class, class> friend class __tree_const_iterator;
};

template <class _Key, class _Tp, class _Compare = less<_Key>,
          class _Allocator = allocator<pair<const _Key, _Tp>>>
class map {
public:
  // types:
  typedef _Key key_type;
  typedef _Tp mapped_type;
  typedef pair<const key_type, mapped_type> value_type;
  typedef __type_identity_t<_Compare> key_compare;
  typedef __type_identity_t<_Allocator> allocator_type;
  typedef value_type &reference;
  typedef const value_type &const_reference;

  static_assert(is_same<typename allocator_type::value_type, value_type>::value,
                "Allocator::value_type must be same type as value_type");

  class value_compare : public __binary_function<value_type, value_type, bool> {
    friend class map;

  protected:
    key_compare comp;

    value_compare(key_compare __c) : comp(__c) {}

  public:
    bool operator()(const value_type &__x, const value_type &__y) const {
      return comp(__x.first, __y.first);
    }
  };

private:
  typedef std::__lldb::__value_type<key_type, mapped_type> __value_type;
  typedef __map_value_compare<key_type, __value_type, key_compare> __vc;
  typedef __rebind_alloc<allocator_traits<allocator_type>, __value_type>
      __allocator_type;
  typedef __tree<__value_type, __vc, __allocator_type> __base;
  typedef typename __base::__node_traits __node_traits;
  typedef allocator_traits<allocator_type> __alloc_traits;

  __base __tree_;

public:
  typedef typename __alloc_traits::pointer pointer;
  typedef typename __alloc_traits::const_pointer const_pointer;
  typedef typename __alloc_traits::size_type size_type;
  typedef typename __alloc_traits::difference_type difference_type;
  typedef __map_iterator<typename __base::iterator> iterator;
  typedef __map_const_iterator<typename __base::const_iterator> const_iterator;
  typedef std::reverse_iterator<iterator> reverse_iterator;
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

  template <class _Key2, class _Value2, class _Comp2, class _Alloc2>
  friend class map;

  explicit map(const key_compare &__comp, const allocator_type &__a)
      : __tree_(__vc(__comp), typename __base::allocator_type(__a)) {}

  ~map() {
    static_assert(sizeof(__diagnose_non_const_comparator<_Key, _Compare>()),
                  "");
  }

  iterator begin() noexcept { return __tree_.begin(); }
  iterator end() noexcept { return __tree_.end(); }
  const_iterator cbegin() const noexcept { return begin(); }
  const_iterator cend() const noexcept { return end(); }

  [[nodiscard]] bool empty() const noexcept { return __tree_.size() == 0; }
  size_type size() const noexcept { return __tree_.size(); }
  size_type max_size() const noexcept { return __tree_.max_size(); }

  mapped_type &operator[](const key_type &__k);

  allocator_type get_allocator() const noexcept {
    return allocator_type(__tree_.__alloc());
  }

  iterator erase(const_iterator __p) { return __tree_.erase(__p.__i_); }
  iterator erase(iterator __p) { return __tree_.erase(__p.__i_); }

private:
  typedef typename __base::__node __node;
  typedef typename __base::__node_allocator __node_allocator;
  typedef typename __base::__node_pointer __node_pointer;
  typedef typename __base::__node_base_pointer __node_base_pointer;
  typedef typename __base::__parent_pointer __parent_pointer;

  typedef __map_node_destructor<__node_allocator> _Dp;
  typedef unique_ptr<__node, _Dp> __node_holder;
};

template <class _Key, class _Tp, class _Compare, class _Allocator>
_Tp &map<_Key, _Tp, _Compare, _Allocator>::operator[](const key_type &__k) {
  return __tree_
      .__emplace_unique_key_args(__k, std::piecewise_construct,
                                 std::forward_as_tuple(__k),
                                 std::forward_as_tuple())
      .first->__get_value()
      .second;
}
} // namespace __lldb
} // namespace std

int main() {
  std::__lldb::map<int, int> v{std::less<int>(),
                               std::allocator<std::pair<int, int>>()};
  v[0] = 5;
  v[10] = 10;
  v[24] = -5;
  v[1] = 0;
  v[-5] = 20;
  auto b = v.begin();
  auto n = std::next(b);
  v.erase(b);
  __builtin_printf("Break here\n");
}
