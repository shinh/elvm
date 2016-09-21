#include <stdio.h>

int main() {
  int a = 43;
  int b = a++;
  int c = ++a;
  int d = b--;
  int e = --a;
  putchar(a);
  putchar(b);
  putchar(c);
  putchar(d);
  putchar(e);

  while (a--) {
    putchar('X');
  }
}
