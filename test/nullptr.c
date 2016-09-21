#include <stdio.h>

#if 0
__attribute__((noinline))
#endif
void* retNull() {
  return 0;
}

int main() {
  if (retNull()) {
    putchar('F');
    putchar('A');
    putchar('I');
    putchar('L');
  } else {
    putchar('O');
    putchar('K');
  }
  return 0;
}
