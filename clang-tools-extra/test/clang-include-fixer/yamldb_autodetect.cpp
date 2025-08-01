// RUN: mkdir -p %t.dir/foo/bar
// RUN: cp %p/Inputs/fake_yaml_db.yaml %t.dir/find_all_symbols_db.yaml
// RUN: cd %t.dir/foo
// RUN: sed -e 's#//.*$##' %s > bar/test.cpp
// RUN: clang-include-fixer -db=yaml bar/test.cpp --
// RUN: FileCheck %s -input-file=bar/test.cpp

// CHECK: #include "foo.h"
// CHECK: b::a::foo f;

b::a::foo f;
