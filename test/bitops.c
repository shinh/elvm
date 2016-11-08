#include <stdio.h>

int main() {
  int a = 22;
  putchar(a ^ 89);
  int b = 1200;
  putchar(b >> 4);
  int c = 0x6f;
  putchar(c & 0x5f);
  int d = 0x42;
  putchar(d | 0x09);
  int e = 0xffffde;
  putchar(~e);
  int f = 5;
  putchar(5 << 1);
  return 0;
}
