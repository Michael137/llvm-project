#include "module1.h"
#include "module2.h"

#include <cstdio>

int main() {
  ClassInMod1 FromMod1;
  ClassInMod2 FromMod2;

  FromMod1.VecInMod1.Member = 137;
  FromMod2.VecInMod2.Member = 42;

  Class2InMod3<char> FromMod1a;
  auto g = global.mem;

  std::puts("Break here");
  return 0;
}
