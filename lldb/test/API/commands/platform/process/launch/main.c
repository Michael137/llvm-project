int main(int argc, char const *argv[]) {
  __builtin_printf("Got %d argument(s).\n", argc);
  for (int i = 0; i < argc; ++i)
    __builtin_printf("[%d]: %s\n", i, argv[i]);
  return 0;
}
