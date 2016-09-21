#include <stdio.h>

int main() {
  while (1) {
    int c = getchar();
    if (c == -1) {
      return 0;
    } else if (c == '(') {
      putchar('p');
    } else if (c == '-' || (c >= '0' && c <= '9')) {
      putchar('n');
    } else {
      putchar('o');
    }
  }
}
