struct foo
{
    int first;
    int second;
};

int main ()
{
    struct foo mine = {0x001122AA, 0x1122BB44};
    __builtin_printf("main.first = 0x%8.8x, main.second = 0x%8.8x\n", mine.first, mine.second);
	mine.first = 0xAABBCCDD; // Set break point at this line.
    __builtin_printf("main.first = 0x%8.8x, main.second = 0x%8.8x\n", mine.first, mine.second);
	mine.second = 0xFF00FF00;
    __builtin_printf("main.first = 0x%8.8x, main.second = 0x%8.8x\n", mine.first, mine.second);
    return 0;
}

