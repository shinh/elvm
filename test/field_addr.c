#include "../libc/libf.h"

struct S {
  int x;
  int y;
};

int main() {
  printf("%d\n", (int)(offsetof(struct S, y) / sizeof(int)));
  return 0;
}
