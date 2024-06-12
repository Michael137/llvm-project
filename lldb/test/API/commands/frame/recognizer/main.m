void foo(int a, int b)
{
    __builtin_printf("%d %d\n", a, b);
}

void bar(int *ptr) { __builtin_printf("%d\n", *ptr); }

int main (int argc, const char * argv[])
{
    foo(42, 56);
    int i = 78;
    bar(&i);
    return 0;
}
