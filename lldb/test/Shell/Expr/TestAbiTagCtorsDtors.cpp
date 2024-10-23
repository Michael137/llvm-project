// Tests that we can call abi-tagged constructors/destructors.

// RUN: %build %s -o %t
// RUN: %lldb %t -o run \
// RUN:          -o "expression sinkTagged(getTagged())" \
// RUN:          -o "expression Tagged()" \
// RUN:          -o exit | FileCheck %s

// CHECK: expression sinkTagged(getTagged())
// CHECK: expression Tagged()

struct Tagged {
  [[gnu::abi_tag("Test", "CtorTag")]] [[gnu::abi_tag("v1")]] Tagged() = default;
  [[gnu::abi_tag("Test", "DtorTag")]] [[gnu::abi_tag("v1")]] ~Tagged() = default;

  int mem = 15;
};

Tagged getTagged() { return Tagged(); }
void sinkTagged(Tagged t) {}

int main() {
  Tagged t;

  // TODO: is there a more reliable way of triggering destructor call?
  sinkTagged(getTagged());
  __builtin_debugtrap();
}

//struct Tagged {
//    // "allocating:_ZN6TaggedC3Ev"
//
//    [[gnu::abi_tag("Test", "CtorTag")]] [[gnu::abi_tag(
//        "v1")]] [[clang::structor_names("complete:_ZN6TaggedC1Ev",
//                                      "base:_ZN6TaggedC2Ev")]] Tagged() =
//        default;
//    [[gnu::abi_tag("Test", "DtorTag")]] [[gnu::abi_tag(
//        "v1")]] [[clang::structor_names("deleting:_ZN6TaggedD0Ev",
//                                      "complete:_ZN6TaggedD1Ev",
//                                      "base:_ZN6TaggedD2Ev")]] ~Tagged();
//
//    int mem = 15;
//};
//
//[[clang::structor_names("deleting:_ZN6TaggedD0Ev",
//"complete:_ZN6TaggedD1Ev",
//"base:_ZN6TaggedD2Ev")]] [[gnu::abi_tag("Test", "DtorTag")]] [[gnu::abi_tag(
//        "v1")]] Tagged::~Tagged() {
//
//}
//
//Tagged getTagged() { return Tagged(); }
//void sinkTagged(Tagged t) {}
//
//int main() {
//    Tagged t;
//
//    // TODO: is there a more reliable way of triggering destructor call?
//    sinkTagged(getTagged());
//    t.~Tagged();
//    __builtin_debugtrap();
//}
