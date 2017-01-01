#ifndef ELVM_LIBC_STDARG_H_
#define ELVM_LIBC_STDARG_H_

typedef char* va_list;
#ifdef __clang__
// TODO: Not sure why we need an increment... but this works.
#define va_start(ap, last) __builtin_va_start(*(__builtin_va_list*)&ap, last), ap++
#else
#define va_start(ap, last) ap = &last
#endif
#define va_arg(ap, type) *(type*)++ap
#define va_copy(dest, src) dest = src
#define va_end(ap)

#endif  // ELVM_LIBC_STDARG_H_
