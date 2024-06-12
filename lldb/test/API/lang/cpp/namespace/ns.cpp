#include "ns.h"

int foo()
{
  __builtin_printf("global foo()\n");
  return 42;
}
int func()
{
  __builtin_printf("global func()\n");
  return 1;
}
int func(int a)
{
  __builtin_printf("global func(int)\n");
  return a + 1;
}
void test_lookup_at_global_scope()
{
  // BP_global_scope
  __builtin_printf("at global scope: foo() = %d\n", foo()); // eval foo(), exp: 42
  __builtin_printf("at global scope: func() = %d\n", func()); // eval func(), exp: 1
}
