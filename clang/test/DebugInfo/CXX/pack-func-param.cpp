// Check that a function parameter pack in a template specialization is emitted
// as DIPackNode with elementTag DW_TAG_formal_parameter and a non-null scope,
// with one DILocalVariable child per expanded argument. The pack variables
// should be unnamed (name is on the DIPackNode) and grouped into retainedNodes.
//
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -emit-llvm -debug-info-kind=standalone \
// RUN:     -gsimple-template-names=simple -std=c++14 %s -o - | FileCheck %s

template<typename... Ts>
void func(int x, Ts... ts) {}

void use() { func(1, 2.0f, 3); }

// The DISubprogram for the specialization should have the pack in retainedNodes.
// CHECK: [[SP:![0-9]+]] = distinct !DISubprogram(name: "func",
// CHECK-SAME: retainedNodes: [[RETAINED:![0-9]+]]
// CHECK: [[RETAINED]] = !{[[PACK:![0-9]+]]}
// CHECK: [[PACK]] = !DIPackNode(elementTag: DW_TAG_formal_parameter, scope: [[SP]], name: "ts", elements: [[ELEMS:![0-9]+]])
// CHECK: [[ELEMS]] = !{[[V1:![0-9]+]], [[V2:![0-9]+]]}
// Pack member variables are unnamed (name is on the DIPackNode).
// CHECK: [[V1]] = !DILocalVariable(arg: {{[0-9]+}}, scope: [[SP]], file:
// CHECK: [[V2]] = !DILocalVariable(arg: {{[0-9]+}}, scope: [[SP]], file:
