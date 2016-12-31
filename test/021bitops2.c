#include <libc/_raw_print.h>

void test_lshift(void) {
  int a = 99;
  int b = 3;
  print_int(a << b);
  putchar('\n');
}

void test_rshift(void) {
  int a = 99;
  int b = 3;
  print_int(a >> b);
  putchar('\n');
}

void test_not(int a) {
  print_int(255 & ~a);
  putchar('\n');
}

int main() {
  test_lshift();
  test_rshift();
  test_not(42);
  return 0;
}
