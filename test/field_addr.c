#include "../libc/libf.h"

struct S {
  int x;
  int y;
};

int main() {
  printf("%d\n", offsetof(struct S, y) / sizeof(int));
  return 0;
}
