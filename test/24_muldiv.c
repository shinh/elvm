#include <stdio.h>

int main() {
  int v = 10777216;
  v *= 9777219;
#ifndef __eir__
  v &= ((1 << 24) - 1);
#endif
  printf("%d\n", v);
  printf("%d\n", 10777216 * 0);
  printf("%d\n", 10777216 / 2);
}
