#include <cstdlib>

int main() {
  __builtin_printf("MY_TEST_ENV_VAR=%s\n", getenv("MY_TEST_ENV_VAR"));

  return 0;
}
