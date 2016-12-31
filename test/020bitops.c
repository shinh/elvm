#include <libc/_raw_print.h>

void test_and(void) {
  int a = 99;
  int b = 45;
  print_int(a & b);
}

void test_or(void) {
  int a = 99;
  int b = 45;
  print_int(a | b);
}

void test_xor(void) {
  int a = 99;
  int b = 45;
  print_int(a ^ b);
}

int main() {
  test_and();
  test_or();
  test_xor();
  return 0;
}
