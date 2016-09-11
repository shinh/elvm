int getchar(void);
int putchar(int c);

int main() {
  while (1) {
    int c = getchar();
    if (c == -1 || c == 0)
      break;
    if (c >= 'A' && c <= 'Z') {
      c += 32;
    } else if (!(c < 'a' || c > 'z')) {
      c -= 32;
    }
    putchar(c);
  }
  return 0;
}
