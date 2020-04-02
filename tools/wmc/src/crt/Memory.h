#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "IoBuffer.h"

#define BITSIZE(t)      (8 * sizeof(t))
typedef uint64_t Cell;

typedef struct _CacheState {
    bool dirty;

    uint64_t head;
    Cell value;
} CacheState;

typedef struct _Memory {
    uint64_t head; // bit address

    Cell *memory;
    uint64_t memorySize;

    CacheState cache;
} Memory;

int
MemInit(
    Memory *State,
    Cell *RawMemory,
    uint64_t RawMemorySize
    );

void
MemAccess(
    Memory *State
    );

void
MemWrite(
    Memory *State,
    bool Bit
    );

bool
MemRead(
    Memory *State
    );
