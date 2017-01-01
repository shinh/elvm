#include <libc/_raw_print.h>

typedef int (*func_t)(void);

int func(void) {
  return 42;
}

void use_func(func_t fp) {
  putchar(fp());
}

int main() {
  use_func(func);
  return 0;
}
