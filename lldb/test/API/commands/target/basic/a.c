int main(int argc, const char* argv[])
{
    int *null_ptr = 0;
    __builtin_printf("Hello, segfault!\n");
    __builtin_printf("Now crash %d\n", *null_ptr); // Crash here.
}
