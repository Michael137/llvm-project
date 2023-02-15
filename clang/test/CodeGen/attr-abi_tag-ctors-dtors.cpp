// RUN: %clang -glldb -S -emit-llvm -o - %s | FileCheck %s

struct Foo {
  [[gnu::abi_tag("Ctor")]] Foo() {}
  [[gnu::abi_tag("Dtor")]] ~Foo() {}
};

struct Bar {
  [[gnu::abi_tag("v1", ".1")]] [[gnu::abi_tag("Ignored1")]] Bar() {}
  [[gnu::abi_tag("v2", ".0")]] [[gnu::abi_tag("Ignored2")]] ~Bar() {}
};

Bar b;
Foo f;

// CHECK:      ![[#]] = !DISubprogram(name: "Foo",
// CHECK-SAME: flags: DIFlagPrototyped, spFlags: [[#]], annotations: ![[FOO_CTOR_ANNOTS:[0-9]+]])
// CHECK:      ![[FOO_CTOR_ANNOTS]] = !{![[CTOR:[0-9]+]]}
// CHECK:      ![[CTOR]] = !{!"abi_tag", !"Ctor"}
// CHECK:      ![[#]] = !DISubprogram(name: "~Foo",
// CHECK-SAME: flags: DIFlagPrototyped, spFlags: [[#]], annotations: ![[FOO_DTOR_ANNOTS:[0-9]+]])
// CHECK:      ![[FOO_DTOR_ANNOTS]] = !{![[DTOR:[0-9]+]]}
// CHECK:      ![[DTOR]] = !{!"abi_tag", !"Dtor"}

// CHECK:      ![[#]] = !DISubprogram(name: "Bar",
// CHECK-SAME: flags: DIFlagPrototyped, spFlags: [[#]], annotations: ![[BAR_CTOR_ANNOTS:[0-9]+]])
// CHECK:      ![[BAR_CTOR_ANNOTS]] = !{![[CTOR:[0-9]+]], ![[TAG:[0-9]+]]}
// CHECK:      ![[CTOR]] = !{!"abi_tag", !".1"}
// CHECK:      ![[TAG]] = !{!"abi_tag", !"v1"}
// CHECK:      ![[#]] = !DISubprogram(name: "~Bar",
// CHECK-SAME: flags: DIFlagPrototyped, spFlags: [[#]], annotations: ![[BAR_DTOR_ANNOTS:[0-9]+]])
// CHECK:      ![[BAR_DTOR_ANNOTS]] = !{![[DTOR:[0-9]+]], ![[TAG:[0-9]+]]}
// CHECK:      ![[DTOR]] = !{!"abi_tag", !".0"}
// CHECK:      ![[TAG]] = !{!"abi_tag", !"v2"}
// CHECK:      ![[#]] = distinct !DISubprogram(name: "Bar", linkageName:
// CHECK-SAME: flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: ![[#]], declaration: ![[#]], retainedNodes: ![[#]])
// CHECK:      ![[#]] = distinct !DISubprogram(name: "~Bar", linkageName:
// CHECK-SAME: flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: ![[#]], declaration: ![[#]], retainedNodes: ![[#]])
// CHECK:      ![[#]] = distinct !DISubprogram(name: "Foo", linkageName:
// CHECK-SAME: flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: ![[#]], declaration: ![[#]], retainedNodes: ![[#]])
// CHECK:      ![[#]] = distinct !DISubprogram(name: "~Foo", linkageName:
// CHECK-SAME: flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: ![[#]], declaration: ![[#]], retainedNodes: ![[#]])
