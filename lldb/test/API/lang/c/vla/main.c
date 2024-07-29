void pause() {}

struct __attribute__((aligned(16))) Foo {
    int x;
};

int foo(int x, int y) {
  int vla[x];
  struct Foo foos[x];
  struct Foo multi_vla[x][y];
  struct Foo multi_vla2[2][3];
  struct Foo foo = {.x=7};

  for (int i = 0; i < x; ++i) {
    vla[i] = x-i;
    struct Foo f = {.x = 5 };
    foos[i] = f;
  }

  for (int i = 0; i < x; ++i)
    for (int j = 0; j < y; ++j) {
      struct Foo f = {.x = x-i-j};
      multi_vla[i][j] = f;
    }

  for (int i = 0; i < 2; ++i)
    for (int j = 0; j < 3; ++j) {
      struct Foo f = {.x = i };
      multi_vla2[i][j] = f;
    }

  int vla0[0];

  pause(); // break here
  return vla[x-1];
}

int main (void) {
  return foo(2, 1) + foo(4, 3);
}
