void bar() {}

template <template <typename K> class... T> void func2(T<int>... ts) { bar(); }

template <typename... T> void func(int x, T... ts) { bar(); }

template <typename T> struct Foo {};

template <template <typename K> class T> struct Bar {};

template <int... Constants> struct Qux {
  template <typename... Types> void qux(Types... ts) { bar(); }
};

int main() {
  char const *s = "hi";
  func<int, const char *, double>(5, 1, s, 1.0);
  func(5, s);
  func2(Foo<int>{});

  Bar<Foo> b;
  Foo<int> f;

  Qux<1, 2, 3>{}.qux<int, double>(5, 1.0f);
  Qux<1, 2, 3>{}.qux(5, 1.0f, s);
  __builtin_debugtrap();
}
