#include <stdbool.h>
#include <stdio.h>

bool ret_bool() {
  return 3;
}

int main() {
  int v = 42;
  bool b = v;
  printf("%d\n", b);
  printf("%d\n", ret_bool());
  return 0;
}
