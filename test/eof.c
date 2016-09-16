#include <stdio.h>

int main() {
  int c = EOF;
#ifndef __eir__
  c &= (1 << 24) - 1;
#endif
  printf("%d\n", c);

  c = fgetc(stdin);
#ifndef __eir__
  c &= (1 << 24) - 1;
#endif
  printf("%d\n", c);

  c = fgetc(stdin);
#ifndef __eir__
  c &= (1 << 24) - 1;
#endif
  printf("%d\n", c);
}
