#include <stdio.h>

int main() {
  while (1) {
    int c = getchar();
    switch (c) {
      case -1:
        return 0;
      case '(':
        putchar('p');
        break;
      case '-':
      case '0':
        putchar('n');
        break;
      default:
        putchar('o');
        break;
    }
  }
}
