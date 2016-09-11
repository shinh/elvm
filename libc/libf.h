static void print_str(const char* p);
static char* stringify_int(long v, char* p);
static void print_int(long v);
static char* stringify_hex(long v, char* p);

#ifdef __GNUC__

#include <ctype.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#else

#define NULL 0

typedef unsigned long size_t;
typedef unsigned long ptrdiff_t;
typedef long off_t;
typedef unsigned char uint8_t;
typedef int bool;
typedef long time_t;
#define true 1
#define false 0
#define offsetof(type, field) ((size_t) &((type *)0)->field)
#define EOF -1

typedef char* va_list;
#define va_start(ap, last) ap = &last
#define va_arg(ap, type) *(type*)++ap
#define va_copy(dest, src) dest = src
#define va_end(ap)

int getchar(void);
int putchar(int c);
int puts(const char* p);
int printf(const char* fmt, ...);
int sprintf(char* buf, const char* fmt, ...);
int snprintf(char* buf, size_t size, const char* fmt, ...);
// We need to declare malloc as int* to reduce bitcasts */
void* malloc(size_t s);
void* calloc(size_t n, size_t s);
void free(void* p);
void exit(int s);

void* memset(void* d, int c, size_t n) {
  size_t i;
  for (i = 0; i < n; i++) {
    ((char*)d)[i] = c;
  }
  return d;
}

void* memcpy(void* d, const void* s, size_t n) {
  size_t i;
  for (i = 0; i < n; i++) {
    ((char*)d)[i] = ((char*)s)[i];
  }
  return d;
}

size_t strlen(const char* s) {
  size_t r;
  for (r = 0; s[r]; r++) {}
  return r;
}

char* strcat(char* d, const char* s) {
  char* r = d;
  for (; *d; d++) {}
  for (; *s; s++, d++)
    *d = *s;
  return r;
}

char* strcpy(char* d, const char* s) {
  char* r = d;
  for (; *s; s++, d++)
    *d = *s;
  return r;
}

int strcmp(const char* a, const char* b) {
  for (;*a || *b; a++, b++) {
    if (*a < *b)
      return -1;
    if (*a > *b)
      return 1;
  }
  return 0;
}

char* strchr(char* s, int c) {
  for (; *s; s++) {
    if (*s == c)
      return s;
  }
  return NULL;
}

typedef struct {
  int quot, rem;
} div_t;

int puts(const char* p) {
  print_str(p);
  putchar('\n');
}

extern int* _edata;

int* malloc(int n) {
  int* r = _edata;
  _edata += n;
  return r;
}

int* calloc(int n, int s) {
  // We assume s is the size of int.
  return malloc(n);
}

void free(void* p) {
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
    switch (*++inp) {
      case 'd':
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
      default:
        print_int(*inp);
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
  char buf[256] = {0};
  int r = vsnprintf(buf, 256, fmt, ap);
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
FILE* stdin;
FILE* stdout;
FILE* stderr;

int fprintf(FILE* fp, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vprintf(fmt, ap);
  va_end(ap);
}

int vfprintf(FILE* fp, const char* fmt, va_list ap) {
  return vprintf(fmt, ap);
}

#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#define MAP_PRIVATE 2
#define MAP_ANON 0x20
void* mmap(void* addr, size_t length, int prot, int flags,
           int fd, off_t offset) {
  return calloc(length, 2);
}

void munmap(void* addr, size_t length) {
}

#define assert(x)                               \
  if (!(x)) {                                   \
    printf("assertion failed: %s\n", #x);       \
    exit(1);                                    \
  }

int isspace(int c) {
  return (c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v');
}

int isdigit(int c) {
  return '0' <= c && c <= '9';
}

int isxdigit(int c) {
  return isdigit(c) || ('a' <= c && c <= 'f') || ('A' <= c && c <= 'F');
}

int isupper(int c) {
  return ('A' <= c && c <= 'Z');
}

int isalpha(int c) {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z');
}

int isalnum(int c) {
  return isalpha(c) || isdigit(c);
}

char* getenv(const char* name) {
  return NULL;
}

int atoi(const char* s) {
  // TODO: Implement?
  assert(0);
}

int fileno(FILE* fp) {
  return 0;
}

int fclose(FILE* fp) {
  return 0;
}

int isatty(int fd) {
  return 1;
}

// From Bionic:

char *
strtok_r(char *s, const char *delim, char **last)
{
  const char *spanp;
  int c, sc;
  char *tok;

  if (s == NULL && (s = *last) == NULL)
    return (NULL);

  /*
   * Skip (span) leading delimiters (s += strspn(s, delim), sort of).
   */
cont:
  c = *s++;
  for (spanp = delim; (sc = *spanp++) != 0;) {
    if (c == sc)
      goto cont;
  }

  if (c == 0) {		/* no non-delimiter characters */
    *last = NULL;
    return (NULL);
  }
  tok = s - 1;

  /*
   * Scan token (scan for delimiters: s += strcspn(s, delim), sort of).
   * Note that delim must have one NUL; we stop if we see that, too.
   */
  for (;;) {
    c = *s++;
    spanp = delim;
    do {
      if ((sc = *spanp++) == c) {
        if (c == 0)
          s = NULL;
        else
          s[-1] = '\0';
        *last = s;
        return (tok);
      }
    } while (sc != 0);
  }
  /* NOTREACHED */
}


/*
 * This array is designed for mapping upper and lower case letter
 * together for a case independent comparison.  The mappings are
 * based upon ascii character sequences.
 */
static const unsigned char charmap[] = {
  '\000', '\001', '\002', '\003', '\004', '\005', '\006', '\007',
  '\010', '\011', '\012', '\013', '\014', '\015', '\016', '\017',
  '\020', '\021', '\022', '\023', '\024', '\025', '\026', '\027',
  '\030', '\031', '\032', '\033', '\034', '\035', '\036', '\037',
  '\040', '\041', '\042', '\043', '\044', '\045', '\046', '\047',
  '\050', '\051', '\052', '\053', '\054', '\055', '\056', '\057',
  '\060', '\061', '\062', '\063', '\064', '\065', '\066', '\067',
  '\070', '\071', '\072', '\073', '\074', '\075', '\076', '\077',
  '\100', '\141', '\142', '\143', '\144', '\145', '\146', '\147',
  '\150', '\151', '\152', '\153', '\154', '\155', '\156', '\157',
  '\160', '\161', '\162', '\163', '\164', '\165', '\166', '\167',
  '\170', '\171', '\172', '\133', '\134', '\135', '\136', '\137',
  '\140', '\141', '\142', '\143', '\144', '\145', '\146', '\147',
  '\150', '\151', '\152', '\153', '\154', '\155', '\156', '\157',
  '\160', '\161', '\162', '\163', '\164', '\165', '\166', '\167',
  '\170', '\171', '\172', '\173', '\174', '\175', '\176', '\177',
  '\200', '\201', '\202', '\203', '\204', '\205', '\206', '\207',
  '\210', '\211', '\212', '\213', '\214', '\215', '\216', '\217',
  '\220', '\221', '\222', '\223', '\224', '\225', '\226', '\227',
  '\230', '\231', '\232', '\233', '\234', '\235', '\236', '\237',
  '\240', '\241', '\242', '\243', '\244', '\245', '\246', '\247',
  '\250', '\251', '\252', '\253', '\254', '\255', '\256', '\257',
  '\260', '\261', '\262', '\263', '\264', '\265', '\266', '\267',
  '\270', '\271', '\272', '\273', '\274', '\275', '\276', '\277',
  '\300', '\301', '\302', '\303', '\304', '\305', '\306', '\307',
  '\310', '\311', '\312', '\313', '\314', '\315', '\316', '\317',
  '\320', '\321', '\322', '\323', '\324', '\325', '\326', '\327',
  '\330', '\331', '\332', '\333', '\334', '\335', '\336', '\337',
  '\340', '\341', '\342', '\343', '\344', '\345', '\346', '\347',
  '\350', '\351', '\352', '\353', '\354', '\355', '\356', '\357',
  '\360', '\361', '\362', '\363', '\364', '\365', '\366', '\367',
  '\370', '\371', '\372', '\373', '\374', '\375', '\376', '\377',
};

int
strcasecmp(const char *s1, const char *s2)
{
  const unsigned char *cm = charmap;
  const unsigned char *us1 = (const unsigned char *)s1;
  const unsigned char *us2 = (const unsigned char *)s2;

  while (cm[*us1] == cm[*us2++])
    if (*us1++ == '\0')
      return (0);
  return (cm[*us1] - cm[*--us2]);
}

int
strncasecmp(const char *s1, const char *s2, size_t n)
{
  if (n != 0) {
    const unsigned char *cm = charmap;
    const unsigned char *us1 = (const unsigned char *)s1;
    const unsigned char *us2 = (const unsigned char *)s2;

    do {
      if (cm[*us1] != cm[*us2++])
        return (cm[*us1] - cm[*--us2]);
      if (*us1++ == '\0')
        break;
    } while (--n != 0);
  }
  return (0);
}

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

#endif

static void print_str(const char* p) {
  for (; *p; p++)
    putchar(*p);
}

static char* stringify_int(long v, char* p) {
  *p = '\0';
  do {
    --p;
    *p = v % 10 + '0';
    v /= 10;
  } while (v);
  return p;
}

static void print_int(long v) {
  char buf[32];
  print_str(stringify_int(v, buf + sizeof(buf) - 1));
}

static char* stringify_hex(long v, char* p) {
  int is_negative = 0;
  int c;
  *p = '\0';
  if (v < 0) {
    v = -v;
    is_negative = 1;
  }
  do {
    --p;
    c = v % 16;
    *p = c < 10 ? c + '0' : c - 10 + 'A';
    v /= 16;
  } while (v);
  if (is_negative)
    *--p = '-';
  return p;
}

// Our 8cc doesn't support returning a structure value.
// TODO: update 8cc.
void my_div(unsigned int a, unsigned int b, div_t* o) {
  unsigned int d[24];
  unsigned int r[24];
  unsigned int i;
  r[0] = 1;
  for (i = 0;; i++) {
    d[i] = b;
    unsigned int nb = b + b;
    if (nb > a || nb < b)
      break;
    r[i+1] = r[i] + r[i];
    b = nb;
  }

  unsigned int q = 0;
  for (;; i--) {
    unsigned int v = d[i];
    if (a >= v) {
      q += r[i];
      a -= v;
    }
    if (i == 0)
      break;
  }
  o->quot = q;
  o->rem = a;
}

static int __builtin_mul(int a, int b) {
  int i, e, v;
  int d[24];
  int r[24];
  for (i = 0, e = 1, v = a;; i++) {
    d[i] = v;
    r[i] = e;
    v += v;
    int ne = e + e;
    if (ne < e || ne > b)
      break;
    e = ne;
  }

  int x = 0;
  for (;; i--) {
    if (b >= r[i]) {
      x += d[i];
      b -= r[i];
    }
    if (i == 0)
      break;
  }
  return x;
}

static unsigned int __builtin_div(unsigned int a, unsigned int b) {
  div_t r;
  my_div(a, b, &r);
  return r.quot;
}

static unsigned int __builtin_mod(unsigned int a, unsigned int b) {
  div_t r;
  my_div(a, b, &r);
  return r.rem;
}
