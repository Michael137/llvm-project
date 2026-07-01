// Check that a template type parameter pack is emitted as DIPackNode with
// elementTag DW_TAG_template_type_parameter, with one DITemplateTypeParameter
// child per expanded argument.
//
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -emit-llvm -debug-info-kind=standalone \
// RUN:     -gsimple-template-names=simple -std=c++14 %s -o - | FileCheck %s

template<typename... Ts>
void func(Ts... ts) {}

void use() { func(1, 2.0f); }

// CHECK: !DISubprogram(name: "func",
// CHECK-SAME: templateParams: [[TPARAMS:![0-9]+]]
// CHECK: [[TPARAMS]] = !{[[PACK:![0-9]+]]}
// CHECK: [[PACK]] = !DIPackNode(elementTag: DW_TAG_template_type_parameter, name: "Ts", elements: [[ELEMS:![0-9]+]])
// CHECK: [[ELEMS]] = !{[[INT:![0-9]+]], [[FLOAT:![0-9]+]]}
// CHECK: [[INT]] = !DITemplateTypeParameter(type: !{{[0-9]+}})
// CHECK: [[FLOAT]] = !DITemplateTypeParameter(type: !{{[0-9]+}})
