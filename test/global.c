#include <stdio.h>

int global = 35;

void foo() {
  global += 34;
}

int main() {
  putchar(global);
  foo();
  putchar(global);
  return 0;
}
