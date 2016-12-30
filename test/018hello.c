int putchar(int c);
int main() {
  const char c[] = "Hello, world!";
  for (int i = 0; i < sizeof(c) - 1; i++)
    putchar(c[i]);
}
