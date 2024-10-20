// Tests that we can call abi-tagged constructors/destructors.

// RUN: %build %s -o %t
// RUN: %lldb %t -o run -o "expression Tagged()" -o exit | FileCheck %s

// CHECK: expression Tagged()
// CHECK: (int) $0 = 15

struct Tagged {
  [[gnu::abi_tag("Test", "Tag")]] Tagged() : mem(12) {

  }

  int mem;
};

int main() {
  Tagged t;
  __builtin_debugtrap();
}
