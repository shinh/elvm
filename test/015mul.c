#ifdef __eir__
#include <_builtin.h>
#endif

int putchar(int c);

int main() {
  int a = 4;
  int b = 12;
  putchar(a * b);
  return 0;
}
