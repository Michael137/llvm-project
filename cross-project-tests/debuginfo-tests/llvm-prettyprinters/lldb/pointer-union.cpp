#include "llvm/ADT/PointerUnion.h"

int main() {
  int a = 5;
  float f = 4.0;
  struct alignas(8) Z {};
  Z z;

  llvm::PointerUnion<Z *, float *> z_float(&f);
  llvm::PointerUnion<Z *, float *> raw_z_float(nullptr);

  llvm::PointerUnion<long long *, int *, float *> long_int_float(&a);
  llvm::PointerUnion<Z *> z_only(&z);

  llvm::PointerIntPair<llvm::PointerUnion<Z *, float *>, 1> union_int_pair(
      z_float, 1);

  // FIXME: should try printing the dynamic type (not just the selected)

  struct Derived : public Z {};
  Derived derived;

  llvm::PointerUnion<Derived *, float *> derived_float(&derived);

  __builtin_debugtrap();

  z_float = &f;

  __builtin_debugtrap();
}
