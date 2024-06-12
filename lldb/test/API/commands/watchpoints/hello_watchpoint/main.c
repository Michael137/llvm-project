#include <stdint.h>

int32_t global = 10; // Watchpoint variable declaration.
char gchar1 = 'a';
char gchar2 = 'b';

int main(int argc, char** argv) {
    int local = 0;
    __builtin_printf("&global=%p\n", &global);
    __builtin_printf("about to write to 'global'...\n"); // Set break point at this line.
                                               // When stopped, watch 'global' for write.
    global = 20;
    gchar1 += 1;
    gchar2 += 1;
    local += argc;
    ++local;
    __builtin_printf("local: %d\n", local);
    __builtin_printf("global=%d\n", global);
    __builtin_printf("gchar1='%c'\n", gchar1);
    __builtin_printf("gchar2='%c'\n", gchar2);
}
