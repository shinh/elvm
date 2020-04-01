#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#include "ProgramCtx.h"
#include "Util.h"

// N.B., This is TIGHTLY coupled with the actual implemention of the compiler
// in elc/target/wm.c. This isn't too big of a deal but something to keep in
// mind.

#define WORD_WIDTH  16
#define BYTE_WIDTH  8

typedef struct _Word {
    uint8_t Data[WORD_WIDTH / BYTE_WIDTH];
} Word;

typedef struct _Control {
    Word Status;
    Word Data;
    Word Address;
    Word Temp; // Copy the working byte here to test for zero
} Control;

typedef struct _Block {
    Control Ctl;
    Word Values[256];
} Block;


void
DumpMemory(
    Memory *Mem
    )
{
    WmPrint("Tape head: %lu\n", Mem->head);
    // TODO Dump cache too maybe

    uint16_t *reg = (uint16_t*) Mem->memory;
    WmPrint("RA: %04x\n", reg[0]);
    WmPrint("RB: %04x\n", reg[1]);
    WmPrint("RC: %04x\n", reg[2]);
    WmPrint("RD: %04x\n", reg[3]);
    WmPrint("BP: %04x\n", reg[4]);
    WmPrint("SP: %04x\n", reg[5]);
    WmPrint("PC: %04x\n", reg[6]);
    WmPrint("CondPC: %04x\n", reg[7]);
    WmPrint("Flags: %04x {", reg[8]);
    {
        if (reg[8] & (1U << 0))
        {
            // ZeroFlag
            WmPrint("Z");
        }
        if (reg[8] & (1U << 1))
        {
            // CarryFlag
            WmPrint("C");
        }
        if (reg[8] & (1U << 2))
        {
            // SignFlag
            WmPrint("S");
        }
        if (reg[8] & (1U << 3))
        {
            // OverflowFlag
            WmPrint("O");
        }
        if (reg[8] & (1U << 4))
        {
            // TrueFlag
            WmPrint("T");
        }
        if (reg[8] & (1U << 5))
        {
            // LoadStoreFlag
            WmPrint("M");
        }
        if (reg[8] & (1U << 6))
        {
            // CondFlag
            WmPrint("?");
        }
    }
    WmPrint("}\n");
    WmPrint("Scratch0: %04x\n", reg[9]);
    WmPrint("AddTmp: %04x\n", reg[10]);
    WmPrint("SubTmp: %04x\n", reg[11]);
    WmPrint("CmpTmp: %04x\n", reg[12]);
    WmPrint("MemValue: %04x\n", reg[13]);
    WmPrint("MemReg: %04x\n", reg[14]);
    WmPrint("ConstantOne: %04x\n", reg[15]);

    Block *blocks = (Block*) &reg[16];
    for (int i = 0; i < 256; ++i)
    {
        Block *block = &blocks[i];

        if (all_zero(&block->Values[0], sizeof(block->Values)))
        {
            //WmPrint("Block %d: empty\n", i);
        }
        else
        {
            ptrdiff_t offset_ptr = ((uint8_t*) block) - ((uint8_t*) &reg[0]);
            uint64_t offset = ((uint64_t) offset_ptr) * 8;
            WmPrint("Block %d (&%lu/0x%lx): ", i, offset, offset);

            {
                uint16_t status = *(uint16_t*) &block->Ctl.Status.Data[0];

                WmPrint("status: {");
                if (status & (1 << 0))
                {
                    // StatusStartingBlock
                    WmPrint(" StartBlock");
                }

                if (status & (1 << 1))
                {
                    // StatusLoadStore (1)
                    WmPrint(" Store");
                }
                else
                {
                    // StatusLoadStore (0)
                    WmPrint(" Load");
                }

                if (status & (1 << 2))
                {
                    // StatusConstantOne
                    WmPrint(" 1");
                }
                if (status & (1 << 3))
                {
                    // StatusCarry
                    WmPrint(" Carry");
                }
                if (status & (1 << 4))
                {
                    // StatusOverflow
                    WmPrint(" Overflow");
                }
                if (status & (1 << 5))
                {
                    // StatusZero
                    WmPrint(" 0");
                }
                WmPrint(" }, ");

            }
            WmPrint("data: %04x, address: %04x, temp: %04x\n",
                *(uint16_t*) &block->Ctl.Data.Data[0],
                *(uint16_t*) &block->Ctl.Address.Data[0],
                *(uint16_t*) &block->Ctl.Temp.Data[0]);

            hexdump("Values", &block->Values[0], sizeof(block->Values));
        }
    }
}

void
DumpBitBuffer(
    BitBuffer *Buf
    )
{
    uint64_t i, offset;

    assert(Buf->BitOffset < BITSIZE(Buf->Buffer));

    WmPrint("Buffer: ");
    for (i = 0; i < BITSIZE(Buf->Buffer); ++i)
    {
        offset = BITSIZE(Buf->Buffer) - i - 1;

        uint64_t bit = Buf->Buffer & (1 << offset);

        if (offset == Buf->BitOffset)
        {
            WmPrint("[%lu]", bit);
        }
        else
        {
            WmPrint("%lu", bit);
        }
    }
    WmPrint("\n");
}

void
DumpIo(
    IoBuffer *Io
    )
{
    WmPrint("Input ");
    DumpBitBuffer(&Io->In);

    WmPrint("Output ");
    DumpBitBuffer(&Io->Out);
}

void
Debug(
    Environment *Env
    )
{
    DumpIo(&Env->io);

    MemAccess(&Env->memory);
    DumpMemory(&Env->memory);

    Breakpoint();
}


#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-label"
void
Program(
    Environment *Env
    )
{
#include "Program.h"

Bail:
    (void) 0;
}
#pragma clang diagnostic pop
