#include <stdio.h>

typedef struct {
  int x;
  int y;
  int z;
} S;

S* struct_ptr = &(S){ .x = 42, .y = 43, .z = 44 };

int main() {
  printf("%d %d %d\n", struct_ptr->x, struct_ptr->y, struct_ptr->z);
  return 0;
}
