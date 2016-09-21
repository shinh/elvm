#include <stdio.h>

int getchar(void);
int putchar(int c);

int func1(int a) {
  return a;
}

int func2(int a, int b) {
  int r = 0;
  putchar(a);
  putchar(b);
  r = a + b;
  return r;
}

int func3(int a, int b, int c) {
  int r = 0;
  putchar(a);
  putchar(b);
  putchar(c);
  r = a + b - c;
  return r;
}

int main() {
  putchar(func1(55));
  putchar(func2(33, 34));
  putchar(func3(48, 49, 50));
  return 0;
}
