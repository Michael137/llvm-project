#include <cstdint>

uint64_t g_foo = 5;
uint64_t g_bar = 6;
uint64_t g_baz = 7;

int main() {
  int val = 8;
  __builtin_printf("Hello world! %d\n", val);
  return 0;
}
