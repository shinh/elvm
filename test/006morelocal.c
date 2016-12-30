int putchar(int c);
int func() {
  return 42;
}
int main() {
  int i = 41;
  int k = 1;
  int a = 1;
  int b = 1;
  int c = 1;
  int m = 1;
  int j = func();
  putchar(i + func() + a + b + c);
  return 0;
}
