int putchar(int c);
int main() {
  int a = 42;
  int h = 43;
  int e = 42;
  int l = 41;
  putchar('0' + (a == h));
  putchar('0' + (a == e));
  putchar('0' + (a == l));
  putchar('0' + (a != h));
  putchar('0' + (a != e));
  putchar('0' + (a != l));
  putchar('0' + (a > h));
  putchar('0' + (a > e));
  putchar('0' + (a > l));
}