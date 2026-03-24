# Pre-delimiter comment.

# RUN: grep 'int main' %{temp}/main.cpp
# RUN: grep 'return 0' %{temp}/main.cpp
# RUN: not grep -c 'Pre-delimiter' %{temp}/main.cpp
# RUN: not grep -c 'foo' %{temp}/main.cpp
# RUN: not grep -c '//---' %{temp}/main.cpp

# RUN: grep foo %{temp}/input.txt
# RUN: grep bar %{temp}/input.txt
# RUN: not grep -c 'Pre-delimiter' %{temp}/input.txt
# RUN: not grep -c 'int main' %{temp}/input.txt
# RUN: not grep -c '//---' %{temp}/input.txt

#--- main.cpp

int main() {
  return 0;
}

#--- input.txt

foo
bar
