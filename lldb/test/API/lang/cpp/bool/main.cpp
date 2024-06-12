int main()
{
  bool my_bool = false;

  __builtin_printf("%s\n", my_bool ? "true" : "false"); // breakpoint 1
}
