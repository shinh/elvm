int putchar(int c);

static int sub(int a, int b) {
  return a - b;
}

int main() {
  int a = 99;
  int b = 12;
  putchar(sub(a, b));
  return 0;
}
