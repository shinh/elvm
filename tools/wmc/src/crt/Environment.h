#pragma once

#include "IoBuffer.h"
#include "Memory.h"

#define TOTAL_BITS  0x10000000
#define MEMORY_SIZE (TOTAL_BITS / (BITSIZE(Cell)))

typedef struct _Environment {
    IoBuffer io;
    Memory memory;
} Environment;


int
EnvironmentInit(
    Environment *Env,
    IoBufferConfig *Io,
    Cell *RawMemory,
    uint64_t RawMemorySize
    );
