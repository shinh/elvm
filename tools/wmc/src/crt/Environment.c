#include <string.h>

#include "Environment.h"

int
EnvironmentInit(
    Environment *Env,
    IoBufferConfig *Io,
    Cell *RawMemory,
    uint64_t RawMemorySize
    )
{
    int status = 0;

    memset(Env, 0, sizeof(*Env));
    CHECK(status = IoBufferInit(&Env->io, Io));
    CHECK(status = MemInit(&Env->memory, RawMemory, RawMemorySize));

Bail:
    return status;
}
