#include <stdio.h>

int main() {
  char buf[16] = {0};
  int n = printf("%d %x %s %c\n", 42, 99, "hello", 'x');
  printf("%d\n", n);
  n = sprintf(buf, "%s", "foobar");
  printf("%s %d\n", buf, n);
  n = snprintf(buf, 3, "hogefuga");
  printf("%s %d\n", buf, n);
  return 0;
}
