int main(int argc, char **argv) {
  __builtin_printf("argc: %d\n", argc);
  return argv[0][0];
}
