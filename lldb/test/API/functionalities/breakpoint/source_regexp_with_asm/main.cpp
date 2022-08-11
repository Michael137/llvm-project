int func_with_asm(void) asm("NonStandardMangling");

int func_with_asm(void) { return 10; }

int main() {
  func_with_asm();
  return 0;
}
