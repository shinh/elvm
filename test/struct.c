#include <stdio.h>

typedef struct {
  int x;
  int y;
} S;

#ifdef __GNUC__
__attribute__((noinline))
#endif
int add(S* s) {
  return s->x + s->y;
}

int main() {
  S s;
  s.x = getchar();
  s.y = getchar();
  putchar(add(&s));
  return 0;
}
