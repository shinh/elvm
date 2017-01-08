#ifndef ELVM_LIBC_SETJMP_H_
#define ELVM_LIBC_SETJMP_H_

#include <stdio.h>
#include <stdlib.h>

typedef void* jmp_buf;

int setjmp(jmp_buf env) { return 0; }
void longjmp(jmp_buf env, int val) {
  fprintf(stderr, "not implemented: longjmp\n");
  exit(1);
}

#endif  // ELVM_LIBC_SETJMP_H_
