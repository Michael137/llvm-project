// clang++ main.cpp -fsyntax-only -Xclang -ast-dump -Xclang -ast-dump-filter=Foo

template<typename T>
struct Foo {
  void method();

  int x;
};

template<>
struct Foo<int> {
  void method();
};

template<>
struct Foo<double> {
  int y;
};


int main() {
    Foo<int> f1;
    Foo<double> f2;
    Foo<char> f3;
    __builtin_debugtrap();
}
