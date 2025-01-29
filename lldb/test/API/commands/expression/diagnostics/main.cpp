#include "header.h"

void foo(int x) {}

struct FooBar {
  int i;
};

enum class EnumInSource { A, B, C };

template <typename T> void TemplateFunc() {}

int main() {
  FooBar f;
  foo(1);
  EnumInSource e = EnumInSource::A;
  TemplateFunc<int>();
  headerFunction();
  return 0; // Break here
}
