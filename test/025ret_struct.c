#include <stdio.h>

typedef struct {
  int a;
  int b;
} S;

S test(void) {
  return (S){ .a = 42, .b = 55 };
}

int main() {
  S s = test();
  printf("%d %d\n", s.a, s.b);
}
