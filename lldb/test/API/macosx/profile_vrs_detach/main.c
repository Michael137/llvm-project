#include <unistd.h>

int
main()
{
  while (1) {
    sleep(1); // Set a breakpoint here
    __builtin_printf("I slept\n");
  }
  return 0;
}
