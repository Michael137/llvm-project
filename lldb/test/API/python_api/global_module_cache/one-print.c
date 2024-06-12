int main() {
  int counter = 0;
  __builtin_printf("I only print one time: %d.\n", counter++);
  return counter;
}
