#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <err.h>
#include <errno.h>
#include <unistd.h>

#include "Util.h"


void
asciify(
    unsigned char *ascii,
    int byte_len
    )
{
    int i;

    for (i = 0; i < byte_len; ++i) {
        if ((ascii[i] < 0x20) || (ascii[i] > 0x7e))
            ascii[i] = '.';
    }
}

void
enhex(
    unsigned char *hexed,
    int hex_size,
    unsigned char *buf,
    int buf_len
    )
{
    int i;

    for (i = 0; i < buf_len; ++i) {
        char tmp[4];
        char *space = (i == 0) ? "" : " ";
        snprintf(tmp, sizeof(tmp), "%s%02x", space, buf[i]);
        strncat((char *)hexed, tmp, hex_size);
    }
}

int
all_zero(
    void *Buf,
    int len
    )
{
    unsigned char *buf = (unsigned char *) Buf;
    int i, isNonZero = 0;
    for (i = 0; i < len; ++i) {
        isNonZero |= buf[i];
    }
    return !isNonZero;
}

void
hexdump(
    char *desc,
    void *addr,
    int len
    )
{
#define BYTE_LEN        (16)
// each byte is two characters (00-FF), plus the whitespace inbetween (BYTE_LEN
// - 1 spaces)
#define BYTE_LEN_HEXED  (BYTE_LEN*2 + BYTE_LEN - 1)

    int i, isZero, beenZero, byte_len, hex_len;
    unsigned char bytes[BYTE_LEN+1], hexed[BYTE_LEN_HEXED+1];
    unsigned char *pc = (unsigned char*)addr;

    if (desc != NULL) {
        WmPrint("%s:\n", desc);
    }

    beenZero = 0;
    for (i = 0; i < len; i += byte_len) {
        hex_len = BYTE_LEN_HEXED;
        byte_len = MIN(BYTE_LEN, len - i);

        hexed[0] = '\0';
        memcpy(bytes, &pc[i], byte_len);
        isZero = all_zero(bytes, byte_len);
        enhex(hexed, hex_len, bytes, byte_len);
        asciify(bytes, byte_len);

        if (!isZero) {
            beenZero = 0;
        }

        if (!beenZero) {
            hexed[hex_len] = '\0';
            bytes[byte_len] = '\0';
            WmPrint("%04x  %-*s  %*s\n", i, hex_len, hexed, byte_len, bytes);

            if (isZero) {
                beenZero = 1;
                WmPrint("*\n");
            }
        }
    }
    WmPrint("%04x\n", i);
}

void
hexdump_only(
    char *desc,
    void *addr,
    int len
    )
{
#define BYTE_LEN        (16)
// each byte is two characters (00-FF), plus the whitespace inbetween (BYTE_LEN
// - 1 spaces)
#define BYTE_LEN_HEXED  (BYTE_LEN*2 + BYTE_LEN - 1)

    int i, byte_len, hex_len;
    unsigned char hexed[BYTE_LEN_HEXED+1];
    unsigned char *pc = (unsigned char*)addr;

    if (desc != NULL) {
        WmPrint("%s:\n", desc);
    }

    for (i = 0; i < len; i += byte_len) {
        hex_len = BYTE_LEN_HEXED;
        byte_len = MIN(BYTE_LEN, len - i);

        hexed[0] = '\0';
        enhex(hexed, hex_len, &pc[i], byte_len);
        hexed[hex_len] = '\0';

        WmPrint("%04x  %-*s\n", i, hex_len, hexed);
    }
    WmPrint("%04x\n", i);
}

inline
void
Breakpoint(
    void
    )
{
    char *envDbg = getenv("DEBUG");

    if (envDbg != NULL)
    {
        __builtin_debugtrap();
        //__asm__("int $3");
    }
}
