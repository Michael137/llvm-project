int main(int argc, char const *argv[]) {
    __builtin_printf("Hello world.\n"); // Set break point at this line.
    if (argc == 1)
        return 0;

    // Waiting to be attached by the debugger, otherwise.
    char line[100];
    while (fgets(line, sizeof(line), stdin)) { // Waiting to be attached...
        __builtin_printf("input line=>%s\n", line);
    }

    __builtin_printf("Exiting now\n");
}
