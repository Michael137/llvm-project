struct C
{
 long c,d;
};

struct Q
{
 long h;
};

struct D
{
};

struct B
{
  [[no_unique_address]] D x;
};

struct E
{
  [[no_unique_address]] D x;
};

struct Foo1 : B,E,C
{
 long a = 42,b = 52;
} _f1;

struct Foo2 : B,E
{
 long v = 42;
} _f2;

struct Foo3 : C,B,E
{
 long v = 42;
} _f3;

struct Foo4 : B,C,E,Q
{
 long v = 42;
} _f4;

struct Foo5 : B,C,E
{
 [[no_unique_address]] D x1;
 [[no_unique_address]] D x2;
 long v1 = 42;
 [[no_unique_address]] D y1;
 [[no_unique_address]] D y2;
 long v2 = 52;
 [[no_unique_address]] D z1;
 [[no_unique_address]] D z2;
} _f5;

struct Foo6 : B,E
{
 long v1 = 42;
 [[no_unique_address]] D y1;
 [[no_unique_address]] D y2;
 long v2 = 52;
} _f6;

int main() {
  return 0; // Set breakpoint here.
}
