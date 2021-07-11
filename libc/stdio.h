#ifndef ELVM_LIBC_STDIO_H_
#define ELVM_LIBC_STDIO_H_

#include <stddef.h>
#include <stdarg.h>
#include "_raw_print.h"
#include <string.h>

#define EOF -1

int getchar(void);
int putchar(int c);
int puts(const char* p);
int printf(const char* fmt, ...);
int sprintf(char* buf, const char* fmt, ...);
int snprintf(char* buf, size_t size, const char* fmt, ...);

int puts(const char* p) {
  print_str(p);
  putchar('\n');
}

int vsnprintf(char* buf, size_t size, const char* fmt, va_list ap) {
  const char* inp;
  size_t off = 0;
  int is_overlow = 0;
  for (inp = fmt; *inp; inp++) {
    if (*inp != '%') {
      if (!is_overlow) {
        if (off + 1 >= size) {
          is_overlow = 1;
          buf[off] = 0;
        } else {
          buf[off] = *inp;
        }
      }
      off++;
      continue;
    }

    char cur_buf[32];
    char* cur_p;
 retry:
    switch (*++inp) {
      case 'l':
        goto retry;
      case 'd':
      case 'u':
        cur_p = stringify_int(va_arg(ap, long), cur_buf + sizeof(cur_buf) - 1);
        break;
      case 'x':
        cur_p = stringify_hex(va_arg(ap, long), cur_buf + sizeof(cur_buf) - 1);
        break;
      case 's':
        cur_p = va_arg(ap, char*);
        break;
      case 'c':
        cur_buf[0] = va_arg(ap, char);
        cur_buf[1] = 0;
        cur_p = cur_buf;
        break;
      case '%':
        cur_buf[0] = '%';
        cur_buf[1] = 0;
        cur_p = cur_buf;
        break;
      default:
        print_int(*inp);
        print_str(" in ");
        print_str(fmt);
        print_str(": unknown format!\n");
        exit(1);
    }

    size_t len = strlen(cur_p);
    if (!is_overlow) {
      if (off + len >= size) {
        is_overlow = 1;
        buf[off] = 0;
      } else {
        strcpy(buf + off, cur_p);
      }
    }
    off += len;
  }
  buf[off] = 0;
  return off;
}

int vsprintf(char* buf, const char* fmt, va_list ap) {
  return vsnprintf(buf, 256, fmt, ap);
}

int snprintf(char* buf, size_t size, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int r = vsnprintf(buf, size, fmt , ap);
  va_end(ap);
  return r;
}

int sprintf(char* buf, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int r = vsprintf(buf, fmt, ap);
  va_end(ap);
  return r;
}

int vprintf(const char* fmt, va_list ap) {
  char buf[256];
  int r = vsnprintf(buf, 256, fmt, ap);
  buf[r] = 0;
  print_str(buf);
  return r;
}

int printf(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int r = vprintf(fmt, ap);
  va_end(ap);
  return r;
}

typedef char FILE;
FILE* stdin = (FILE*)1;
FILE* stdout = (FILE*)1;
FILE* stderr = (FILE*)1;

int fprintf(FILE* fp, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vprintf(fmt, ap);
  va_end(ap);
}

int vfprintf(FILE* fp, const char* fmt, va_list ap) {
  return vprintf(fmt, ap);
}

int fileno(FILE* fp) {
  return 0;
}

FILE* fopen(const char* filename, const char* mode) {
  return stdin;
}

int fclose(FILE* fp) {
  return 0;
}

size_t fwrite(void* ptr, size_t s, size_t n, FILE* fp) {
  char* str = ptr;
  size_t l = (int)s * (int)n;
  for (size_t i = 0; i < l; i++)
    putchar(str[i]);
  return l;
}

int fputs(const char* s, FILE* fp) {
  print_str(s);
}

static int g_ungot = EOF;
static int eof_seen;

int fgetc(FILE* fp) {
  int r;
  if (g_ungot == EOF) {
    // A hack for whitespace, in which getchar after EOF is not
    // defined well.
    if (eof_seen)
      return EOF;
    r = getchar();
    eof_seen = r == EOF;
    return r;
  }
  r = g_ungot;
  g_ungot = EOF;
  return r;
}

int getc(FILE* fp) {
  return fgetc(fp);
}

int ungetc(int c, FILE* fp) {
  if (g_ungot == EOF)
    return g_ungot = c;
  return EOF;
}

int fputc(int c, FILE* fp) {
  return putchar(c);
}

int putc(int c, FILE* fp) {
  return putchar(c);
}

char* fgets(char* s, int size, FILE* fp) {
  if (size <= 0)
    return NULL;

  int i = 0;
  while (i < size - 1) {
    int c = fgetc(fp);
    if (c == EOF) {
      if (i) break;
      return NULL;
    }
    s[i++] = c;
    if (c == '\n')
      break;
  }
  s[i] = 0;
  return s;
}

#endif  // ELVM_LIBC_STDIO_H_
