int putchar(int x);

int main() {
    
  const char* p = "fib result is \n";
   for (; *p; p++)
    putchar(*p);

  char res = fb(1);

  putchar(res);
  return 0;
}

char fb(int n) {
    if(n == 0) {
        return '1';
    } else if (n == 1) {
        return '1';
    }

    return fb(n-1) + fb(n-2);
}