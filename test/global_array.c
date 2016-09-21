#include <stdio.h>

int bss;
int val;
int arr[99];

int main() {
  val = 9;
  arr[0] = 10;
  arr[1] = 11;
  arr[2] = 12;
  putchar(bss + val + arr[0] + arr[1] + arr[2] + arr[3]);
  return 0;
}
