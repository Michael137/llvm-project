#include "a.h"

int
main_func(int input)
{
  return __builtin_printf("Set B breakpoint here: %d.\n", input);
}

int
main()
{
  a_func(10);
  main_func(10);
  __builtin_printf("Set a breakpoint here:\n");
  return 0;
}
