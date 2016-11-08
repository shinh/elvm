#ifndef ELVM_LIBC_UNISTD_H_
#define ELVM_LIBC_UNISTD_H_

#include <stddef.h>

typedef long off_t;

#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#define MAP_PRIVATE 2
#define MAP_ANON 0x20

void* mmap(void* addr, size_t length, int prot, int flags,
           int fd, long offset) {
  return calloc(length, 2);
}

void munmap(void* addr, size_t length) {
}

int isatty(int fd) {
  return 1;
}

#endif  // ELVM_LIBC_UNISTD_H_
