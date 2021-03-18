#ifndef ELVM_LIBC_STRING_H_
#define ELVM_LIBC_STRING_H_

#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>

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
  *d = 0;
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

int strncmp(const char* a, const char* b, size_t n) {
  int i;
  for (i=0; (*a || *b) && i < n; a++, b++, i++) {
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

char* strdup(const char* s) {
  int l = strlen(s);
  char* r = malloc(l + 1);
  strcpy(r, s);
  return r;
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

// From Bionic:
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

// From Bionic:
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

// From Bionic:
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

// From Bionic:
char *
strpbrk(const char *s1, const char *s2)
{
	const char *scanp;
	int c, sc;

	while ((c = *s1++) != 0) {
		for (scanp = s2; (sc = *scanp++) != 0;)
			if (sc == c)
				return ((char *)(s1 - 1));
	}
	return (NULL);
}

// From Bionic:
size_t
strcspn(const char *s1, const char *s2)
{
	const char *p, *spanp;
	char c, sc;
	/*
	 * Stop as soon as we find any character from s2.  Note that there
	 * must be a NULL in s2; it suffices to stop when we find that, too.
	 */
	for (p = s1;;) {
		c = *p++;
		spanp = s2;
		do {
			if ((sc = *spanp++) == c)
				return (p - 1 - s1);
		} while (sc != 0);
	}
	/* NOTREACHED */
}
#endif  // ELVM_LIBC_STRING_H_
