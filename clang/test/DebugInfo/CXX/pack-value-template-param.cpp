// Check that a non-type template parameter pack is emitted as DIPackNode with
// elementTag DW_TAG_template_value_parameter, with one DITemplateValueParameter
// child per expanded argument.
//
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -emit-llvm -debug-info-kind=standalone \
// RUN:     -gsimple-template-names=simple -std=c++14 %s -o - | FileCheck %s

template<int... Vs>
struct S {};

S<1, 2, 3> s;

// CHECK: !DICompositeType(tag: DW_TAG_structure_type, name: "S",
// CHECK-SAME: templateParams: [[TPARAMS:![0-9]+]]
// CHECK: [[TPARAMS]] = !{[[PACK:![0-9]+]]}
// CHECK: [[PACK]] = !DIPackNode(elementTag: DW_TAG_template_value_parameter, name: "Vs", elements: [[ELEMS:![0-9]+]])
// CHECK: [[ELEMS]] = !{[[V1:![0-9]+]], [[V2:![0-9]+]], [[V3:![0-9]+]]}
// CHECK: [[V1]] = !DITemplateValueParameter(type: !{{[0-9]+}}, value: i32 1)
// CHECK: [[V2]] = !DITemplateValueParameter(type: !{{[0-9]+}}, value: i32 2)
// CHECK: [[V3]] = !DITemplateValueParameter(type: !{{[0-9]+}}, value: i32 3)
