typedef struct { int x; int y; } S;
int putchar(int c);
int main() {
  S s;
  s.x = 42;
  s.y = 43;
  putchar(s.x);
  putchar(s.y);
}
