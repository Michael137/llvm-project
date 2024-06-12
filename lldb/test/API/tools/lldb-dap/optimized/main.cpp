#include <string>

int foo(int x, int y) {
  __builtin_printf("Got input %d, %d\n", x, y);
  return x + y + 3; // breakpoint 1
}

int main(int argc, char const *argv[]) {
  __builtin_printf("argc: %d\n", argc);
  int result = foo(20, argv[0][0]);
  __builtin_printf("result: %d\n", result); // breakpoint 2
  return 0;
}
