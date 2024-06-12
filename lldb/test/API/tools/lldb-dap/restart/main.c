int main(int argc, char const *argv[], char const *envp[]) {
  int i = 0;
  __builtin_printf("Do something\n"); // breakpoint A
  __builtin_printf("Do something else\n");
  i = 1234;
  return 0; // breakpoint B
}
