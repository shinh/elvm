#ifndef ELVM_LIBC_LOCALE_H_
#define ELVM_LIBC_LOCALE_H_

#define LC_ALL 6

char* setlocale(int category, const char* locale) {
  return locale;
}

#endif  // ELVM_LIBC_LOCALE_H_
