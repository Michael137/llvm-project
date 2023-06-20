// Test for debug info for C++ defaulted member functions

// RUN: %clang_cc1 -emit-llvm -triple x86_64-linux-gnu %s -o - \
// RUN:   -O0 -debug-info-kind=standalone -std=c++20 | FileCheck %s

// CHECK:     DISubprogram(name: "defaulted", {{.*}}, flags: DIFlagPrototyped, spFlags: DISPFlagDefaultedInClass)
// CHECK:     DISubprogram(name: "~defaulted", {{.*}}, flags: DIFlagPrototyped, spFlags: DISPFlagDefaultedOutOfClass)
// CHECK:     DISubprogram(name: "operator=", {{.*}}, flags: DIFlagPrototyped, spFlags: 0)
// CHECK:     DISubprogram(name: "operator=", {{.*}}, flags: DIFlagPrototyped, spFlags: 0)
// CHECK:     DISubprogram(name: "operator==", {{.*}}, flags: DIFlagPrototyped, spFlags: DISPFlagDefaultedInClass)
// CHECK-NOT: DISubprogram(name: "implicit_defaulted"
struct defaulted {
  // inline defaulted
  defaulted() = default;

  // out-of-line defaulted (inline keyword
  // shouldn't change that)
  inline ~defaulted();

  // These shouldn't produce a defaulted-ness DI flag
  // (though technically they are DW_DEFAULTED_no)
  defaulted& operator=(defaulted const&) { return *this; }
  defaulted& operator=(defaulted &&);

  bool operator==(defaulted const&) const = default;
};

defaulted::~defaulted() = default;
defaulted& defaulted::operator=(defaulted &&) { return *this; }

// All ctors/dtors are implicitly defatuled.
// So no DW_AT_defaulted expected for these.
struct implicit_defaulted {};

void foo() {
  defaulted d;
  implicit_defaulted i;
}
