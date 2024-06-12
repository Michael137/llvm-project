void
doSomething()
{
  __builtin_printf ("Set a breakpoint here.\n");
  __builtin_printf ("Need a bit more code.\n");
}

int
main()
{
  doSomething();
  return 0;
}
