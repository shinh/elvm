#include <stdio.h>

int main() {
  const char* p = "Hello, world!\n";
  for (; *p; p++)
    putchar(*p);
  return 0;
}
