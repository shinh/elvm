#include <stdio.h>

int main() {
  putchar(__builtin_mul(22, 5));
  int a = 22;
  putchar(a * 5);
  int b = 99;
  putchar(b / 3);
  int c = 999;
  putchar(c % 100);
  return 0;
}
