#include <cassert>
#include <cstdio>

int global_var = -5;

struct Baz {
  virtual int baz_virt() = 0;

  int base_base_var = 12;
};

struct Bar : public Baz {
  virtual int baz_virt() override {
    base_var = 10;
    return 1;
  }

  int base_var = 15;
};

struct foo : public Bar {
  int class_var = 9;
  int shadowed;

  virtual int baz_virt() override {
    method2();
    return 2;
  }

  int *class_ptr;

  struct foo_nested {
    void nested_method() {}

    double nested;
  };

  void method2() { shadowed = -1; }

  void method() {
    int local_var = 137;
    int shadowed;
    foo_nested f;
    class_ptr = &local_var;
    f.nested_method();
    auto lambda = [&shadowed, this, p = this, &local_var,
                   local_var_copy = local_var]() mutable {
      int lambda_local_var = 5;
      shadowed = 5;
      class_var = 109;
      --base_var;
      --base_base_var;
      std::puts("break here");

      auto nested_lambda = [this, &lambda_local_var] {
        std::puts("break here");
        lambda_local_var = 0;
      };

      nested_lambda();
      --local_var_copy;
      std::puts("break here");

      struct LocalLambdaClass {
        int lambda_class_local = -12345;
        foo *outer_ptr;

        void inner_method() {
          auto lambda = [this] {
            std::puts("break here");
            lambda_class_local = -2;
            outer_ptr->class_var *= 2;
          };

          lambda();
        }
      };

      LocalLambdaClass l;
      l.outer_ptr = this;
      l.inner_method();
      std::puts("break here");
    };
    lambda();
  }
};

int main() {
  foo f;
  f.method();
  return 0; // break here
}
