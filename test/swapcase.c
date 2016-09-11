int getchar(void);
int putchar(int c);

int main() {
  int c = getchar();
  if (c >= 'A' && c <= 'Z') {
    c += 32;
  } else if (c >= 'a' && c <= 'z') {
    c -= 32;
  }
  putchar(c);
  return 0;
}
