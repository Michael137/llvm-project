void foo() {
  __builtin_printf("hello world from foo"); // Set break point at this line.
}

int main() {
  foo();
  return 0;
}
