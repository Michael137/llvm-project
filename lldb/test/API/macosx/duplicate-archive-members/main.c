extern int a(int);
extern int b(int);
int main (int argc, char const *argv[])
{
    __builtin_printf ("a(1) returns %d\n", a(1));
    __builtin_printf ("b(2) returns %d\n", b(2));
}
