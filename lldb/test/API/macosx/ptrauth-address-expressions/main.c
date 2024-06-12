int foo () { return 10; }

int main () 
{
  int (*fptr)() = foo;
  __builtin_printf ("%p\n", fptr); // break here
  return fptr();
}
