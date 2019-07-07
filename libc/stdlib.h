#ifndef ELVM_LIBC_STDLIB_H_
#define ELVM_LIBC_STDLIB_H_

#include <stddef.h>
#include "_raw_print.h"
#include <ctype.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

static void print_str(const char* p);

typedef struct {
  int quot, rem;
} div_t;

extern int* _edata;

void exit(int s);

void abort(void) {
  exit(1);
}

void* malloc(int n) {
  int* r = _edata;
  _edata += n;
  if (r > _edata) {
    print_str("no memory!\n");
    exit(1);
  }
  return r;
}

int* calloc(int n, int s) {
  return malloc(n * s);
}

void free(void* p) {
}

// From Bionic:
long
strtol(const char *nptr, char **endptr, int base)
{
  const char *s;
  long acc, cutoff;
  int c;
  int neg, any, cutlim;

  /*
   * Ensure that base is between 2 and 36 inclusive, or the special
   * value of 0.
   */
  if (base < 0 || base == 1 || base > 36) {
    if (endptr != 0)
      *endptr = (char *)nptr;
    return 0;
  }

  /*
   * Skip white space and pick up leading +/- sign if any.
   * If base is 0, allow 0x for hex and 0 for octal, else
   * assume decimal; if base is already 16, allow 0x.
   */
  s = nptr;
  do {
    c = (unsigned char) *s++;
  } while (isspace(c));
  if (c == '-') {
    neg = 1;
    c = *s++;
  } else {
    neg = 0;
    if (c == '+')
      c = *s++;
  }
  if ((base == 0 || base == 16) &&
      c == '0' && (*s == 'x' || *s == 'X')) {
    c = s[1];
    s += 2;
    base = 16;
  }
  if (base == 0)
    base = c == '0' ? 8 : 10;

  if (neg) {
    if (cutlim > 0) {
      cutlim -= base;
      cutoff += 1;
    }
    cutlim = -cutlim;
  }
  for (acc = 0, any = 0;; c = (unsigned char) *s++) {
    if (isdigit(c))
      c -= '0';
    else if (isalpha(c))
      c -= isupper(c) ? 'A' - 10 : 'a' - 10;
    else
      break;
    if (c >= base)
      break;
    if (any < 0)
      continue;
    if (neg) {
      any = 1;
      acc *= base;
      acc -= c;
    } else {
      any = 1;
      acc *= base;
      acc += c;
    }
  }
  if (endptr != 0)
    *endptr = (char *) (any ? s - 1 : nptr);
  return (acc);
}

unsigned long strtoul(const char *nptr, char **endptr, int base) {
  return strtol(nptr, endptr, base);
}

long long strtoll(const char *nptr, char **endptr, int base) {
  return strtol(nptr, endptr, base);
}

unsigned long long strtoull(const char *nptr, char **endptr, int base) {
  return strtol(nptr, endptr, base);
}

int atoi(const char* s) {
  int n = 0;
  unsigned long i = 0;
  while (s[i] >= '0' && s[i] <= '9') {
    n = (n << 3) + (n << 1);
    n += s[i++] - '0';
  }
  return n;
}

char* getenv(const char* name) {
  return NULL;
}

void qsort(void* vbase, size_t nmemb, size_t size,
           int (*compar)(const void*, const void*)) {
  if (nmemb <= 1)
    return;
  char* base = (char*)vbase;
  char* pivot = base + (nmemb / 2) * size;
  char* left = base;
  char* right = base + (nmemb - 1) * size;
#if 0
  printf("nmemb=%d size=%d\n", nmemb, size);
  for (int i = 0; i < nmemb; i++) {
    printf("%d ", *((int*)base + i));
  }
  puts("");
#endif
  for (;;) {
    //printf("left=%d right=%d", left-base, right-base);
    while (compar(left, pivot) >= 32768)
      left += size;
    while ((compar(right, pivot) - 1) < 32768)
      right -= size;
    //printf(" => left=%d right=%d\n", left-base, right-base);
    if (left >= right)
      break;
    for (size_t i = 0; i < size; i++) {
      char tmp = left[i];
      left[i] = right[i];
      right[i] = tmp;
    }
    if (pivot == left)
      pivot = right;
    else if (pivot == right)
      pivot = left;
    left += size;
    right -= size;
  }
  //left -= size;
  right += size;
  qsort(base, (size_t)(left - base) / size, size, compar);
  qsort(right, (size_t)(base + nmemb * size - right) / size,
        size, compar);
}

#endif  // ELVM_LIBC_STDLIB_H_
