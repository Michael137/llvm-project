int a_function_to_call()
{ 
    return 0;
}

int main()
{
    __builtin_printf("%d\n", a_function_to_call()); // breakpoint
}
