#ifndef ELVM_LIBC_STDINT_H_
#define ELVM_LIBC_STDINT_H_

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;
typedef unsigned long uintptr_t;
typedef char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef long int64_t;
typedef long intptr_t;
typedef long time_t;

#define INT8_MAX	0x7f
#define INT8_MIN	(-INT8_MAX - 1)
#define UINT8_MAX	(INT8_MAX * 2 + 1)
#define INT16_MAX	0x7fff
#define INT16_MIN	(-INT16_MAX - 1)
#define UINT16_MAX	(0x7fffU * 2U + 1U)
#define INT32_MAX	0x7fffffffL
#define INT32_MIN	(-INT32_MAX - 1L)
#define UINT32_MAX	(0x7fffffffLU * 2UL + 1UL)
#define INT64_MAX	0x7fffffffffffffffLL
#define INT64_MIN	(-INT64_MAX - 1LL)
#define UINT64_MAX	(0x7fffffffffffffffLLU * 2ULL + 1ULL)

#endif  // ELVM_LIBC_STDINT_H_
