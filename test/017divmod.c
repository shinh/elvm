#ifdef __eir__
#include <_builtin.h>
#endif

int putchar(int c);

int main() {
  int a = 9949;
  int b = 99;
  //putchar(a / b);
  putchar(a % b);
  return 0;
}
