#ifndef MOD3_H_IN
#define MOD3_H_IN

template <typename SIZE_T> struct ClassInMod3Base { int BaseMember = 0; };

template <typename T> struct ClassInMod3 : public ClassInMod3Base<int> {};

#endif // _H_IN
