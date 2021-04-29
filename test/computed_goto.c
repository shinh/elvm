#include <stdio.h>

void* label_return;

int main() {
  void* label_array[] = {&&label_a, &&label_b, &&label_c};

  int label_array_relative[] = {
    &&label_a - &&label_a,
    &&label_b - &&label_a,
    &&label_c - &&label_a,
  };

label_0:
  label_return = &&label_1;
  goto *label_array[0];

label_1:
  label_return = &&label_2;
  goto *label_array[1];

label_2:
  label_return = &&label_3;
  goto *label_array[2];

label_3:
  label_return = &&label_4;
  goto *(label_array_relative[0] + &&label_a);

label_4:
  label_return = &&label_5;
  goto *(label_array_relative[1] + &&label_a);

label_5:
  label_return = &&label_6;
  goto *(label_array_relative[2] + &&label_a);

label_6:
  goto *(&&label_end);

label_a:
  printf("label_a\n");
  goto *label_return;

label_b:
  printf("label_b\n");
  goto *label_return;

label_c:
  printf("label_c\n");
  goto *label_return;

label_end:
  return 0;
}
