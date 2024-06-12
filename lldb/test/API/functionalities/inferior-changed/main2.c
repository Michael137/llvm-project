#include <stdlib.h>

int main(int argc, const char* argv[])
{
    int *int_ptr = (int *)malloc(sizeof(int));
    *int_ptr = 7;
    __builtin_printf("Hello, world!\n");
    __builtin_printf("Now not crash %d\n", *int_ptr); // Not crash here.
}
