extern int a(int);
extern int b(int);
extern int c(int);
int main (int argc, char const *argv[])
{
    __builtin_printf ("a(1) returns %d\n", a(1));
    __builtin_printf ("b(2) returns %d\n", b(2));
    __builtin_printf ("c(2) returns %d\n", c(2));
}
