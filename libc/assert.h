#ifndef ELVM_LIBC_ASSERT_H_
#define ELVM_LIBC_ASSERT_H_

#include "_raw_print.h"

void exit(int s);

#define assert(x)                               \
  if (!(x)) {                                   \
    print_str("assertion failed: " #x "\n");    \
    exit(1);                                    \
  }

#endif  // ELVM_LIBC_ASSERT_H_
