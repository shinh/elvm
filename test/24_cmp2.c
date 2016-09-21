#include <stdio.h>

int main() {
  unsigned int a = 1048575;
  unsigned int b = 2621440;
  printf("%d\n", b < a); //
  printf("%d\n", b > a);
  printf("%d\n", b <= a);
  printf("%d\n", b >= a); //
  printf("%d\n", a < b);
  printf("%d\n", a > b); //
  printf("%d\n", a <= b); //
  printf("%d\n", a >= b);
}
