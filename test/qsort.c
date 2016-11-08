#include <stdio.h>
#include <stdlib.h>

int cmp_int(const void* a, const void* b) {
  return *(int*)a - *(int*)b;
}

typedef struct {
  int x, y;
} S;

int cmp_s(const void* a, const void* b) {
  S* sa = (S*)a;
  S* sb = (S*)b;
  return sa->x * sa->y - sb->x * sb->y;
}

void test1() {
  puts("test1");
  int data[] = {
    3, 5, 7, 1, 4, 2
  };
  qsort(data, 6, sizeof(int), cmp_int);
  for (int i = 0; i < 6; i++) {
    printf("%d\n", data[i]);
  }
}

void test2() {
  puts("test2");
  int data[] = {
    3, 5, 7, 5, 1, 4, 2
  };
  qsort(data, 7, sizeof(int), cmp_int);
  for (int i = 0; i < 7; i++) {
    printf("%d\n", data[i]);
  }
}

void test3() {
  puts("test3");
  S data[] = {
    { 3, 3 },
    { 5, 2 },
    { 7, 1 },
    { 5, 4 },
    { 1, 11 },
    { 4, 3 },
    { 2, 1 }
  };
  qsort(data, 7, sizeof(S), cmp_s);
  for (int i = 0; i < 7; i++) {
    printf("%d = %d * %d\n", data[i].x * data[i].y, data[i].x, data[i].y);
  }
}

int main() {
  test1();
  test2();
  test3();
}
