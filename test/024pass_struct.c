#include <stdio.h>

typedef struct {
  int a;
  int b;
} S;

void test(int x, S s, int y) {
  printf("%d %d %d %d\n", y, s.a, s.b, x);
}

int main() {
  S s = (S){ .a = 42, .b = 55 };
  test(0, s, 99);
}
