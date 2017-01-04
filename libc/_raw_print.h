#ifndef ELVM_LIBC_RAW_PRINT_H_
#define ELVM_LIBC_RAW_PRINT_H_

#include "_builtin.h"

int putchar(int c);

static void print_str(const char* p) {
  for (; *p; p++)
    putchar(*p);
}

static char* stringify_int(unsigned int v, char* p) {
  *p = '\0';
  do {
    --p;
    *p = v % 10 + '0';
    v /= 10;
  } while (v);
  return p;
}

static void print_int(unsigned int v) {
  char buf[32];
  print_str(stringify_int(v, buf + sizeof(buf) - 1));
}

static char* stringify_hex(unsigned int v, char* p) {
  int is_negative = 0;
  int c;
  *p = '\0';
  do {
    --p;
    c = v % 16;
    *p = c < 10 ? c + '0' : c - 10 + 'a';
    v /= 16;
  } while (v);
  return p;
}

#endif  // ELVM_LIBC_RAW_PRINT_H_
