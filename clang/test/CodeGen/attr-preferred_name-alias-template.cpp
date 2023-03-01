// RUN: %clang -target x86_64 -glldb -S -emit-llvm -o - %s | FileCheck %s --check-prefixes=LLDB,COMMON
// RUN: %clang -target x86_64 -ggdb -S -emit-llvm -o - %s | FileCheck %s --check-prefixes=GDB,COMMON

template<typename T>
struct Foo;

template<typename T>
using Bar = Foo<T>;

template<typename T>
struct [[clang::preferred_name(Bar<int>),
         clang::preferred_name(Bar<double>)]] Foo {};

Foo<int> varInt;
Foo<double> varDouble;
Foo<char> varChar;

// COMMON:      ![[FOO_DOUBLE:[0-9]+]] = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "Foo<double>"
// GDB-NOT:     preferredName: [[#]]
// LLDB-SAME:   preferredName: ![[DOUBLE_PREF:[0-9]+]]
// COMMON-SAME: )

// LLDB:      ![[DOUBLE_PREF]] = !DIDerivedType(tag: DW_TAG_typedef, name: "Bar<double>",
// LLDB-SAME: baseType: ![[FOO_DOUBLE]]

// COMMON:       !DICompositeType(tag: DW_TAG_structure_type, name: "Foo<char>"
// GDB-NOT:      preferredName: [[#]]
// LLDB-NOT:     preferredName: [[#]]
// COMMON-SAME:  )

// COMMON:      ![[FOO_INT:[0-9]+]] = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "Foo<int>"
// GDB-NOT:     preferredName: [[#]]
// LLDB-SAME:   preferredName: ![[INT_PREF:[0-9]+]]
// COMMON-SAME: )

// LLDB:      ![[INT_PREF]] = !DIDerivedType(tag: DW_TAG_typedef, name: "Bar<int>",
// LLDB-SAME: baseType: ![[FOO_INT]]
