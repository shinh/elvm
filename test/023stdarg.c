#include <libc/_raw_print.h>
#include <stdarg.h>

void print_sum(int s, ...) {
  va_list ap;
  va_start(ap, s);
  while (1) {
    int v = va_arg(ap, int);
    print_int(v);
    putchar('\n');
    if (v == 0)
      break;
    s += v;
  }
  va_end(ap);
  print_int(s);
  putchar('\n');
  putchar('\n');
}

void print_sum2(int d, int s, ...) {
  va_list ap;
  va_start(ap, s);
  while (1) {
    int v = va_arg(ap, int);
    print_int(v);
    putchar('\n');
    if (v == 0)
      break;
    s += v;
  }
  va_end(ap);
  print_int(s - d);
  putchar('\n');
  putchar('\n');
}

int main() {
  print_sum(4, 5, 9, 0);
  print_sum2(11, 4, 5, 9, 0);
  return 0;
}
