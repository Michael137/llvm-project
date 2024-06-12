#include "pseudo_barrier.h"
#include <thread>

volatile uint32_t g_val = 0;
pseudo_barrier_t g_barrier, g_barrier2;

void thread_func() {
  pseudo_barrier_wait(g_barrier);
  pseudo_barrier_wait(g_barrier2);
  __builtin_printf("%s starting...\n", __FUNCTION__);
  for (uint32_t i = 0; i < 10; ++i)
    g_val = i;
}

int main(int argc, char const *argv[]) {
  __builtin_printf("Before running the thread\n");
  pseudo_barrier_init(g_barrier, 2);
  pseudo_barrier_init(g_barrier2, 2);
  std::thread thread(thread_func);

  __builtin_printf("After launching the thread\n");
  pseudo_barrier_wait(g_barrier);

  __builtin_printf("After running the thread\n");
  pseudo_barrier_wait(g_barrier2);

  thread.join();

  return 0;
}
