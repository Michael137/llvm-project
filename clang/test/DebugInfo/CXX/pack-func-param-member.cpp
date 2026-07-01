// Check that a function parameter pack on a member function template is
// emitted as DIPackNode on both the declaration and definition subprograms.
//
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -emit-llvm -debug-info-kind=standalone \
// RUN:     -gsimple-template-names=simple -std=c++14 %s -o - | FileCheck %s

struct Foo {
  template<typename... Ts>
  void bar(Ts... ts) {}
};

void use() { Foo{}.bar(1, 2.0f); }

// The definition SP appears first in the IR. Capture the declaration ref and
// the definition's retainedNodes ref for later matching.
// CHECK: distinct !DISubprogram(name: "bar", {{.*}}spFlags: DISPFlagDefinition,
// CHECK-SAME: declaration: [[DECL:![0-9]+]], retainedNodes: [[DEF_NODES:![0-9]+]]

// Immediately after comes the declaration SP. Its retainedNodes contains only
// the DIPackNode. Both declaration and definition use the same arg numbering:
// 'this' is arg 1 (implicit), explicit params start at arg 2.
// CHECK: [[DECL]] = !DISubprogram(name: "bar", {{.*}}spFlags: 0,
// CHECK-SAME: retainedNodes: [[DECL_NODES:![0-9]+]]
// CHECK: [[DECL_NODES]] = !{[[DECL_PACK:![0-9]+]]}
// CHECK: [[DECL_PACK]] = !DIPackNode(elementTag: DW_TAG_formal_parameter, scope: [[DECL]], name: "ts", elements: [[DECL_ELEMS:![0-9]+]])
// CHECK: [[DECL_ELEMS]] = !{[[DECL_V1:![0-9]+]], [[DECL_V2:![0-9]+]]}
// CHECK: [[DECL_V1]] = !DILocalVariable(arg: 2, scope: [[DECL]],
// CHECK: [[DECL_V2]] = !DILocalVariable(arg: 3, scope: [[DECL]],

// The definition's retainedNodes also contains only the pack node.
// Pack members are arg 2/3 because 'this' is arg 1.
// CHECK: [[DEF_NODES]] = !{[[DEF_PACK:![0-9]+]]}
// CHECK: [[DEF_PACK]] = !DIPackNode(elementTag: DW_TAG_formal_parameter, scope: {{![0-9]+}}, name: "ts", elements: [[DEF_ELEMS:![0-9]+]])
// CHECK: [[DEF_ELEMS]] = !{[[V1:![0-9]+]], [[V2:![0-9]+]]}
// CHECK: [[V1]] = !DILocalVariable(arg: 2, scope: {{![0-9]+}}, file:
// CHECK: [[V2]] = !DILocalVariable(arg: 3, scope: {{![0-9]+}}, file:
