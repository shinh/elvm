#include "../libc/_raw_print.h"

int main() {
  int i;
  for (i = 1; i <= 100; i++) {
    int done = 0;
    if (i % 3 == 0) {
      putchar('F');
      putchar('i');
      putchar('z');
      putchar('z');
      done = 1;
    }
    if (i % 5 == 0) {
      putchar('B');
      putchar('u');
      putchar('z');
      putchar('z');
      done = 1;
    }

    if (!done)
      print_int(i);
    putchar('\n');
  }
  return 0;
}
