#pragma once

#include "Environment.h"

// ops

#define SEEKL(amount) (Env->memory.head -= amount)
#define SEEKR(amount) (Env->memory.head += amount)

#define SET()       MemWrite(&Env->memory, true)
#define UNSET()     MemWrite(&Env->memory, false)

#define JMP(t, f)   {                               \
    if (MemRead(&Env->memory)) {                    \
        goto t;                                     \
    } else {                                        \
        goto f;                                     \
    }                                               \
}

#define INPUT()     {                               \
    bool bit;                                       \
    CHECK(IoBufferGetBit(&Env->io, &bit));          \
    MemWrite(&Env->memory, bit);                    \
}
#define OUTPUT()    {                               \
    bool bit;                                       \
    bit = MemRead(&Env->memory);                    \
    CHECK(IoBufferPutBit(&Env->io, bit));           \
}

#define DEBUG()     {                               \
    Debug(Env);                                     \
}


void
DumpMemory(
    Memory *Mem
    );

void
Program(
    Environment *Env
    );
