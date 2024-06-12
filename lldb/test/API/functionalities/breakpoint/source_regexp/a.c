#include "a.h"

static int
main_func(int input)
{
  return __builtin_printf("Set B breakpoint here: %d", input);
}

int
a_func(int input)
{
  input += 1; // Set A breakpoint here;
  return main_func(input);
}
