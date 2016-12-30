int putchar(int c);
int sub(int a, int b) {
  return a - b;
}
int main() {
  putchar(sub(99, 42));
  return 0;
}
