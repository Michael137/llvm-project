int main(int argc, char *argv[]) {
  for (int i = 0; i < argc; i++) {
    __builtin_printf("argv[%d] = \"%s\"\n", i, argv[i]);
  }
  return 0;
}
