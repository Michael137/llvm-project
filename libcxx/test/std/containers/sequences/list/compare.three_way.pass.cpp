//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// UNSUPPORTED: c++03, c++11, c++14, c++17

// <list>

// template <class T, class Allocator> constexpr
//   synth-three-way-result<T>
//     operator<=>(const list<T, Allocator>& x, const list<T, Allocator>& y); // constexpr since C++26

#include <list>
#include <cassert>

#include "test_container_comparisons.h"

int main(int, char**) {
  assert(test_sequence_container_spaceship<std::list>());
#if TEST_STD_VER >= 26
  static_assert(test_sequence_container_spaceship<std::list>());
#endif
  return 0;
}
