typedef struct Struct
{
  int one;
  int two;
} Struct;

int
main()
{
  Struct myStruct = {10, 20};
  __builtin_printf ("Break here: %d\n.", myStruct.one);
  return 0;
}
