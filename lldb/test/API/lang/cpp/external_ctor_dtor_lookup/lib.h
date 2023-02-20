#ifndef LIB_H_IN
#define LIB_H_IN

template <typename T> class Wrapper {
public:
  [[gnu::abi_tag("test", "ctor")]] Wrapper(){};

  [[gnu::abi_tag("test", "dtor")]] ~Wrapper(){};
};

struct Foo {};

Wrapper<Foo> getFooWrapper();

#endif // _H_IN
