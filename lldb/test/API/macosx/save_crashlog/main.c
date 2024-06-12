void
call_me() {
  __builtin_printf("I was called");
}

int
main()
{
  call_me();
  return 0;
}
