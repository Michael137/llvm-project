void foo() {
  int foo = 10; 
  __builtin_printf("%d\n", foo); // Set a breakpoint here. 
  foo = 20;
  __builtin_printf("%d\n", foo);
}

int main() {
  foo();
  return 0;
}
