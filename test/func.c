int getchar(void);
int putchar(int c);
int swap(int c);

int main() {
  while (1) {
    int c = getchar();
    if (c == -1)
      break;
    putchar(swap(c));
  }
  return 0;
}

int swap(int c) {
  if (c >= 'A' && c <= 'Z') {
    return c + 32;
  } else if (c >= 'a' && c <= 'z') {
    return c - 32;
  }
  return c;
}
