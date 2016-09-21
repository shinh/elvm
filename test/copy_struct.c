#include <stdio.h>
#include <stdlib.h>

typedef struct {
  int x;
  int y;
  int z;
} S;

int main() {
  S s1 = { 42, 44, 46 };
  S s2;
  s2 = s1;
  printf("%d %d %d\n", s2.x, s2.y, s2.z);
  // TODO: There's an upstream bug in 8cc.
  //S s3 = s1;
  //printf("%d %d %d\n", s3.x, s3.y, s3.z);
  S* s4 = malloc(sizeof(S));
  *s4 = s1;
  printf("%d %d %d\n", s4->x, s4->y, s4->z);
  return 0;
}
