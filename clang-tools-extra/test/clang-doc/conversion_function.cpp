// RUN: rm -rf %t && mkdir -p %t

// RUN: clang-doc --output=%t --executor=standalone %s 
// RUN: find %t/ -regex ".*/[0-9A-F]*.yaml" -exec cat {} ";" | FileCheck %s --check-prefix=CHECK-YAML

// RUN: clang-doc --format=html --output=%t --executor=standalone %s 
// RUN: FileCheck %s < %t/GlobalNamespace/MyStruct.html --check-prefix=CHECK-HTML

template <typename T>
struct MyStruct {
  operator T();
};

// Output correct conversion names.
// CHECK-YAML:         Name:            'operator T'

// CHECK-HTML: <h3 id="{{[0-9A-F]*}}">operator T</h3>
// CHECK-HTML: <p>public T operator T()</p>
