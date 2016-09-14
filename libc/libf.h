static void print_str(const char* p);
static char* stringify_int(long v, char* p);
static void print_int(long v);
static char* stringify_hex(long v, char* p);

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __GNUC__

#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#endif
