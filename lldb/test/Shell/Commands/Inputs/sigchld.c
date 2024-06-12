#include <assert.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>

void handler(int signo) {
  __builtin_printf("SIGCHLD\n");
}

int main() {
  void *ret = signal(SIGINT, handler);
  assert (ret != SIG_ERR);

  pid_t child_pid = fork();
  assert (child_pid != -1);

  if (child_pid == 0) {
    sleep(1);
    _exit(14);
  }

  __builtin_printf("signo = %d\n", SIGCHLD);
  __builtin_printf("code = %d\n", CLD_EXITED);
  __builtin_printf("child_pid = %d\n", child_pid);
  __builtin_printf("uid = %d\n", getuid());
  pid_t waited = wait(NULL);
  assert(waited == child_pid);

  return 0;
}
