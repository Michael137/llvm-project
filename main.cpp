// clang++ main.cpp -fsyntax-only -Xclang -ast-dump -Xclang -ast-dump-filter=func

template<typename T>
void func(int x = 10) {}

int main() {
    func<int>();
    return 0;
}
