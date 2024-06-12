#include "ns.h"

static int func()
{
  __builtin_printf("static m2.cpp func()\n");
  return 2;
}
void test_lookup_at_file_scope()
{
  // BP_file_scope
  __builtin_printf("at file scope: func() = %d\n", func()); // eval func(), exp: 2
  __builtin_printf("at file scope: func(10) = %d\n", func(10)); // eval func(10), exp: 11
}
namespace A {
    namespace B {
        int func()
        {
          __builtin_printf("A::B::func()\n");
          return 4;
        }
        void test_lookup_at_nested_ns_scope()
        {
            // BP_nested_ns_scope
            __builtin_printf("at nested ns scope: func() = %d\n", func()); // eval func(), exp: 4

            //__builtin_printf("func(10) = %d\n", func(10)); // eval func(10), exp: 13
            // NOTE: Under the rules of C++, this test would normally get an error
            // because A::B::func() hides A::func(), but lldb intentionally
            // disobeys these rules so that the intended overload can be found
            // by only removing duplicates if they have the same type.
        }
        void test_lookup_at_nested_ns_scope_after_using()
        {
            // BP_nested_ns_scope_after_using
            using A::func;
            __builtin_printf("at nested ns scope after using: func() = %d\n", func()); // eval func(), exp: 3
        }
    }
}
int A::foo()
{
  __builtin_printf("A::foo()\n");
  return 42;
}
int A::func(int a)
{
  __builtin_printf("A::func(int)\n");
  return a + 3;
}
void A::test_lookup_at_ns_scope()
{
  // BP_ns_scope
  __builtin_printf("at nested ns scope: func() = %d\n", func()); // eval func(), exp: 3
  __builtin_printf("at nested ns scope: func(10) = %d\n", func(10)); // eval func(10), exp: 13
  __builtin_printf("at nested ns scope: foo() = %d\n", foo()); // eval foo(), exp: 42
}
