#ifndef ELVM_LIBC_STDARG_H_
#define ELVM_LIBC_STDARG_H_

typedef char* va_list;
#define va_start(ap, last) ap = &last
#define va_arg(ap, type) *(type*)++ap
#define va_copy(dest, src) dest = src
#define va_end(ap)

#endif  // ELVM_LIBC_STDARG_H_
