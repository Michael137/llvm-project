void foo()
{
  __builtin_printf("foo()\n");
}

int bar()
{
  int ret = 3;
  __builtin_printf("bar()->%d\n", ret);
  return ret;
}

void baaz(int i)
{
  __builtin_printf("baaz(%d)\n", i);
}
