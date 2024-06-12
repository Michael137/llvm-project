static int var = 5;
int main() {
  __builtin_printf("%p is %d\n", &var, var);
  return ++var;
}
