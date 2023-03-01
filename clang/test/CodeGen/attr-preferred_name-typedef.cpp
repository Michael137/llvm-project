// RUN: %clang -target x86_64 -glldb -S -emit-llvm -o - %s | FileCheck %s --check-prefixes=LLDB,COMMON
// RUN: %clang -target x86_64 -ggdb -S -emit-llvm -o - %s | FileCheck %s --check-prefixes=GDB,COMMON

template<typename T>
struct Foo;

typedef Foo<int> BarInt;
typedef Foo<double> BarDouble;
typedef Foo<char> BarChar;

template<typename T>
struct [[clang::preferred_name(BarInt),
         clang::preferred_name(BarDouble)]] Foo {};

Foo<int> varInt;
Foo<double> varDouble;
Foo<char> varChar;

// COMMON:    ![[FOO_DOUBLE:[0-9]+]] = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "Foo<double>"
// GDB-NOT:   preferredName: [[#]]
// LLDB-SAME: preferredName: ![[DOUBLE_PREF:[0-9]+]])
// COMMON:    )

// LLDB:      !DIDerivedType(tag: DW_TAG_typedef, name: "BarDouble",
// LLDB-SAME: baseType: ![[FOO_DOUBLE]])

// COMMON:    !DICompositeType(tag: DW_TAG_structure_type, name: "Foo<char>"
// GDB-NOT:   preferredName: [[#]]
// LLDB-NOT:  preferredName: [[#]]
// COMMON:    )

// COMMON:    ![[FOO_INT:[0-9]+]] = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "Foo<int>"
// GDB-NOT:   preferredName: [[#]]
// LLDB-SAME: preferredName: ![[INT_PREF:[0-9]+]])
// COMMON:    )

// LLDB:      !DIDerivedType(tag: DW_TAG_typedef, name: "BarInt",
// LLDB-SAME: baseType: ![[FOO_INT]])
