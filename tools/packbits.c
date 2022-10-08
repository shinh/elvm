#include <stdio.h>

int main (void) {
    char c;
    char buf = 0;
    int mask = 1 << 7;
    int n = 0;
    while ((c = getchar()) != EOF) {
        if (c & 1) {
            buf |= mask;
        }
        mask >>= 1;
        n++;
        if (n == 8) {
            putchar(buf);
            buf = 0;
            mask = 1 << 7;
            n = 0;
        }
    }
    if (n > 0) {
        putchar(buf);
    }
    return 0;
}
