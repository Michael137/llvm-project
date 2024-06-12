void
call_me(int value) {
  __builtin_printf("called with %d\n", value); // Set another here.
}

int
main(int argc, char **argv)
{
  call_me(argc); // Set a breakpoint here.
  __builtin_printf("This just spaces the two calls\n");
  call_me(argc); // Run here to step over again.
  __builtin_printf("More spacing\n");
  return 0; // Make sure we get here on last continue
}
