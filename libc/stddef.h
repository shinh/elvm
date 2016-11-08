#ifndef ELVM_LIBC_STDDEF_H_
#define ELVM_LIBC_STDDEF_H_

#include "_builtin.h"

#define NULL 0

typedef unsigned long size_t;
typedef unsigned long ptrdiff_t;

#define offsetof(type, field) ((size_t) &((type *)0)->field)

#endif  // ELVM_LIBC_STDDEF_H_
