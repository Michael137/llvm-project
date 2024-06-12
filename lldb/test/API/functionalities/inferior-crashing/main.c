const char *hello_world = "Hello, segfault!";

int main(int argc, const char* argv[])
{
    int *null_ptr = 0;
    __builtin_printf("%s\n", hello_world);
    __builtin_printf("Now crash %d\n", *null_ptr); // Crash here.
}
