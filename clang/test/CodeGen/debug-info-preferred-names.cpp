// RUN: %clang_cc1 -emit-llvm -debug-info-kind=standalone %s -o - -debugger-tuning=lldb | FileCheck --check-prefixes=COMMON,LLDB %s
// RUN: %clang_cc1 -emit-llvm -debug-info-kind=standalone %s -o - -debugger-tuning=gdb | FileCheck --check-prefixes=COMMON,GDB %s

template<typename T>
class Qux {};

template<typename T>
struct Foo;

template<typename T>
using Bar = Foo<T>;

template<typename T>
struct [[clang::preferred_name(Bar<T>)]] Foo {};

int main() {
    /* Trivial cases */

    Bar<int> b;
// COMMON: !DIDerivedType(tag: DW_TAG_typedef, name: "Bar<int>"

    Foo<int> f1;
// COMMON: !DICompositeType(tag: DW_TAG_structure_type, name: "Foo<int>"

    /* Alias template case */

    Bar<Foo<int>> f2;
// GDB:    !DIDerivedType(tag: DW_TAG_typedef, name: "Bar<Foo<int> >"
// LLDB:   !DIDerivedType(tag: DW_TAG_typedef, name: "Bar<Bar<int> >"

    /* Nested cases */

    Foo<Foo<int>> f3; 
// GDB:    !DICompositeType(tag: DW_TAG_structure_type, name: "Foo<Foo<int> >"
// LLDB:   !DICompositeType(tag: DW_TAG_structure_type, name: "Foo<Bar<int> >"
    
    Qux<Foo<int>> f4;
// GDB:    !DICompositeType(tag: DW_TAG_class_type, name: "Qux<Foo<int> >"
// LLDB:   !DICompositeType(tag: DW_TAG_class_type, name: "Qux<Bar<int> >"

    return 0;
}
