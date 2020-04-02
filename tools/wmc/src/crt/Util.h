#pragma once

#include <stdint.h>
#include <assert.h>

#include <err.h>

#define ARRAY_SIZE(array) (sizeof(array)/sizeof(array[0]))

#define MIN(a, b)   (((a) < (b)) ? (a) : (b))
#define MAX(a, b)   (((a) < (b)) ? (b) : (a))

#define PAGE_ROUND_UP(x)    ( (((ulong)(x)) + PAGE_SIZE-1)  & (~(PAGE_SIZE-1)) )

#define PRESENT(_Value, _Bits)  (((_Value) & (_Bits)) == (_Bits))


#define FAILED(x)       ((x) < 0)
#define SUCCEEDED(x)    (!FAILED(x))

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression"

#define CHECK(Status)       \
if (FAILED(Status)) {       \
    goto Bail;              \
}

#define BAIL(Status)  {     \
    Status;                 \
    goto Bail;              \
}

#pragma clang diagnostic pop

//
// N.B., the bitfield layout is NOT guaranteed so we need to have tests or
// runtime checks to ensure Failure maps to the highest bit.
//
/*
typedef union _STATUS {
    struct {
        uint32_t Code       : 16;
        uint32_t Subsystem  : 15;
        uint32_t Failure    : 1;
    };

    uint32_t AsUint32;
} STATUS;
*/

#define MAKE_CODE(Code)             (((Code)      & ((1 << 16) - 1)) << 0)
#define MAKE_SUBSYSTEM(Subsystem)   (((Subsystem) & ((1 << 15) - 1)) << 16)
#define FAILURE_BIT                 (1 << 31)
#define MAKE_STATUS(Code, Subsystem) (FAILURE_BIT | (MAKE_SUBSYSTEM(Subsystem)) | (MAKE_CODE(Code)))
typedef int32_t     STATUS;

//
// New subsystems should use this template.
//

#define ENVIRONMENT                 1
#define ENVIRONMENT_STATUS(Code)    (MAKE_STATUS(Code, ENVIRONMENT))

#define ENV_OK              0
#define ENV_INT_OVERFLOW    (ENVIRONMENT_STATUS(1))
#define ENV_OOM             (ENVIRONMENT_STATUS(2))
#define ENV_BADPTR          (ENVIRONMENT_STATUS(3))
#define ENV_BADARG          (ENVIRONMENT_STATUS(4))
#define ENV_BADLEN          (ENVIRONMENT_STATUS(5))
#define ENV_LENTOOBIG       (ENVIRONMENT_STATUS(6))
#define ENV_BADENUM         (ENVIRONMENT_STATUS(7))
#define ENV_EOF             (ENVIRONMENT_STATUS(8))
#define ENV_NOTIMPLEMENTED  (ENVIRONMENT_STATUS(9))
#define ENV_FAILURE         (ENVIRONMENT_STATUS(10))
#define ENV_DUPLICATE       (ENVIRONMENT_STATUS(11))
#define ENV_MISSING         (ENVIRONMENT_STATUS(12))

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"

#define WmPrint(fmt, ...)      fprintf(stderr, fmt, ##__VA_ARGS__)
#define WmWarn(fmt, ...)       warn ("%s: " fmt, __FUNCTION__, ##__VA_ARGS__)
#define WmWarnx(fmt, ...)      warnx("%s: " fmt, __FUNCTION__, ##__VA_ARGS__)

#pragma clang diagnostic pop

int
all_zero(
    void *buf,
    int len
    );

void
hexdump(
    char *desc,
    void *addr,
    int len
    );

void
hexdump_only(
    char *desc,
    void *addr,
    int len
    );

void
Breakpoint(
    void
    );
