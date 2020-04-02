#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "Util.h"

typedef int(*GetByteFn)(void *);
typedef int(*PutByteFn)(int, void *);

typedef struct _BitBuffer
{
    uint8_t Buffer, BitOffset;
} BitBuffer;

typedef struct _IoBufferConfig
{
    GetByteFn GetByte;
    void *GetContext;

    PutByteFn PutByte;
    void *PutContext;
} IoBufferConfig;

typedef struct _IoBuffer
{
    BitBuffer In;
    BitBuffer Out;

    // XXX Should this encompass both in and out?
    bool Eof;

    IoBufferConfig Config;
} IoBuffer;

int
IoBufferInit(
    IoBuffer *IoBuf,
    IoBufferConfig *Config
    );

int
IoBufferGetBit(
    IoBuffer *IoBuf,
    void *In
    );

int
IoBufferPutBit(
    IoBuffer *IoBuf,
    int Out
    );
