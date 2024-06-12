int
test_func()
{
  return __builtin_printf("I am a test function.");
}

void
break_on_me()
{
  __builtin_printf("I was called.\n");
}

int
main()
{
  break_on_me();
  test_func();
  return 0;
}
