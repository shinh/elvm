int getchar(void);
int putchar(int c);

int main() {
  int c = getchar();
  if (c == 'A')
    putchar('X');
  else
    putchar('Y');
  putchar('\n');
  return 0;
}
