int main() {
  int counter = 0;
  __builtin_printf("I print one time: %d.\n", counter++);
  __builtin_printf("I print two times: %d.\n", counter++);
  return counter;
}
