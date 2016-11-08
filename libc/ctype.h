#ifndef ELVM_LIBC_CTYPE_H_
#define ELVM_LIBC_CTYPE_H_

int isspace(int c) {
  return (c == '\f' || c == '\n' || c == '\r' ||
          c == '\t' || c == '\v' || c == ' ');
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

int isprint(int c) {
  return isspace(c) || (c >= 32 && c < 127);
}

#endif  // ELVM_LIBC_CTYPE_H_
