#include <stdio.h>

int main() {
  char buf[256];
  char* ptr = fgets(buf, 100, stdin);
  printf("%d\n", ptr == buf);
  printf("%s\n", ptr);
  ptr = fgets(buf, 100, stdin);
  printf("%d\n", ptr == buf);
  printf("%s\n", ptr);
  ptr = fgets(buf, 100, stdin);
  printf("%d\n", ptr == 0);
  return 0;
}
