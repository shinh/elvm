#include <stdio.h>

int main() {
  for (int i = 1; i <= 100; i++) {
    if (i % 5) {
      if (i % 3) {
        printf("%d\n", i);
      } else {
        printf("Fizz\n");
      }
    } else {
      printf("FizzBuzz\n" + i * i % 3 * 4);
    }
  }
  return 0;
}
