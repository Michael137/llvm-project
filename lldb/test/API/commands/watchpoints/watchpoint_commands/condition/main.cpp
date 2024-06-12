#include <stdint.h>

int32_t global = 0; // Watchpoint variable declaration.

static void modify(int32_t &var) {
    ++var;
}

int main(int argc, char** argv) {
    int local = 0;
    __builtin_printf("&global=%p\n", &global);
    __builtin_printf("about to write to 'global'...\n"); // Set break point at this line.
                                               // When stopped, watch 'global',
                                               // for the condition "global == 5".
    for (int i = 0; i < 10; ++i)
        modify(global);

    __builtin_printf("global=%d\n", global);
}
