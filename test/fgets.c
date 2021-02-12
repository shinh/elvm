#include <stdio.h>

int main() {
  char buf[256];
  char* ptr;

  ptr = fgets(buf, 100, stdin);
  printf("%d %s\n", ptr == buf, ptr);

  ptr = fgets(buf, 100, stdin);
  printf("%d %s\n", ptr == buf, ptr);

  buf[0] = 'a';
  ptr = fgets(buf, 100, stdin);
  printf("%d %d\n", ptr == 0, buf[0]);

  buf[0] = 'b';
  ptr = fgets(buf, 1, stdin);
  printf("%d %d\n", ptr == 0, buf[0]);

  buf[0] = 'c';
  ptr = fgets(buf, 100, stdin);
  printf("%d %d\n", ptr == 0, buf[0]);

  buf[0] = 'd';
  ptr = fgets(buf, 0, stdin);
  printf("%d %d\n", ptr == 0, buf[0]);

  return 0;
}
