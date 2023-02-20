#include "lib.h"

struct Bar {
  Wrapper<Foo> getWrapper() { return Wrapper<Foo>(); }
  int sinkWrapper(Wrapper<Foo>) { return -1; }
};

struct A {
  int m_int = -1;
  float m_float = -1.0;
  [[gnu::abi_tag("Ctor", "Int")]] A(int a) : m_int(a) {}
  [[gnu::abi_tag("Ctor", "Float")]] A(float f) : m_float(f) {}
  [[gnu::abi_tag("Ctor", "Default")]] A() {}
  [[gnu::abi_tag("Dtor", "Default")]] ~A() {}
};

struct B : virtual A {
  [[gnu::abi_tag("TagB")]] B();
};
B::B() : A(47) {}

struct D : B {
  [[gnu::abi_tag("TagD")]] D() : A(4.7f) {}
};

struct X : public virtual A {};

int main() {
  Bar b;
  Wrapper<int> w1;
  Wrapper<double> w2;
  Wrapper<Foo> w3 = getFooWrapper();
  Wrapper<Foo> w4;
  B b2;
  D d;
  A a;
  auto *ptr = new X();
  delete ptr;
  return b.sinkWrapper(b.getWrapper());
}
