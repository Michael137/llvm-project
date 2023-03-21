#ifndef MOD3_H_IN
#define MOD3_H_IN

template<typename T> struct Foo;

typedef Foo<char> Bar;

template<typename T> struct [[clang::preferred_name(Bar)]] Foo {};

template <typename T> struct Baz { Foo<char> member; };

template <typename SIZE_T> struct ClassInMod3 { int Member = 0; Foo<char> foo; };

template <typename SIZE_T> struct Class2InMod3 { ClassInMod3<char> mem; };

typedef Class2InMod3<char> Qux;

inline Qux global;

#endif // _H_IN
