int global_test_var = 10;

int
main()
{
  int test_var = 10;
  __builtin_printf ("Set a breakpoint here: %d.\n", test_var);
  return global_test_var;
}
