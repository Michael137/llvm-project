int
call_me(int input)
{
  return __builtin_printf("I was called: %d.\n", input);
}

int
main()
{
  int test_var = 10;
  __builtin_printf ("Set a breakpoint here: %d.\n", test_var);
  return call_me(100);
}
