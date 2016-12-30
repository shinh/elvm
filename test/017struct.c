int putchar(int c);
typedef struct {
  int a;
  int s;
} S;
int main() {
  S s;
  s.s = 42;
  s.a = 99;
  putchar(s.s);
}
