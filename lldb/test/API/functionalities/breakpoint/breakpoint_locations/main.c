#define INLINE inline __attribute__((always_inline))

int
func_not_inlined (void)
{
    __builtin_printf ("Called func_not_inlined.\n");
    return 0;
}

INLINE int
func_inlined (void)
{
    static int func_inline_call_count = 0;
    __builtin_printf ("Called func_inlined.\n");
    ++func_inline_call_count;  // Set break point at this line.
    __builtin_printf ("Returning func_inlined call count: %d.\n", func_inline_call_count);
    return func_inline_call_count;
}

extern int func_inlined (void);

int
main (int argc, char **argv)
{
  __builtin_printf ("Starting...\n");

  int (*func_ptr) (void);
  func_ptr = func_inlined;

  int a = func_inlined();
  __builtin_printf("First call to func_inlined() returns: %d.\n", a);

  func_not_inlined ();

  func_ptr ();

  __builtin_printf("Last call to func_inlined() returns: %d.\n", func_inlined ());
  return 0;
}


