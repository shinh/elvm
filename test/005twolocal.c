int putchar(int c);
int func() {
  return 4;
}
int main() {
  int i = 41;
  int j = func();
  putchar(i + func() + j);
  return 0;
}
