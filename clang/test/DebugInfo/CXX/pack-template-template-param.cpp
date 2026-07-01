// Check that a template template parameter pack is emitted as DIPackNode with
// elementTag DW_TAG_GNU_template_template_param, with one
// DITemplateValueParameter(tag: DW_TAG_GNU_template_template_param) child per
// expanded argument.
//
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -emit-llvm -debug-info-kind=standalone \
// RUN:     -gsimple-template-names=simple -std=c++14 %s -o - | FileCheck %s

template<typename T> struct Foo {};

template<template<typename> class... Ts>
void func(Ts<int>... ts) {}

void use() { func(Foo<int>{}); }

// CHECK: !DISubprogram(name: "func",
// CHECK-SAME: templateParams: [[TPARAMS:![0-9]+]]
// CHECK: [[TPARAMS]] = !{[[PACK:![0-9]+]]}
// CHECK: [[PACK]] = !DIPackNode(elementTag: DW_TAG_GNU_template_template_param, name: "Ts", elements: [[ELEMS:![0-9]+]])
// CHECK: [[ELEMS]] = !{[[FOO:![0-9]+]]}
// CHECK: [[FOO]] = !DITemplateValueParameter(tag: DW_TAG_GNU_template_template_param, value: !"Foo")
