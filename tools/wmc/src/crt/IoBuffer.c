#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <err.h>

#include <assert.h>

#include "IoBuffer.h"
#include "Util.h"

#define LAST    8
#define INITIAL 0

#define EXTRACT_BIT(Data, Bit) (((Data) >> (Bit)) & 1)
#define EMPLACE_BIT(Data, Bit) (((Data) & 1) << (Bit))


int
IoBufferInit(
    IoBuffer *IoBuf,
    IoBufferConfig *Config
    )
{
    int status = 0;

    if (IoBuf == NULL)
    {
        status = ENV_BADPTR;
        WmWarnx("NULL IoBuf pointer");
        goto Bail;
    }

    if (Config == NULL)
    {
        status = ENV_BADPTR;
        WmWarnx("NULL Config pointer");
        goto Bail;
    }

    if (Config->GetByte == NULL)
    {
        status = ENV_BADPTR;
        WmWarnx("NULL GetByte function pointer");
        goto Bail;
    }

    if (Config->PutByte == NULL)
    {
        status = ENV_BADPTR;
        WmWarnx("NULL PutByte function pointer");
        goto Bail;
    }

    memset(IoBuf, 0, sizeof(*IoBuf));
    IoBuf->Config = *Config;

Bail:
    return status;
}

int
IoBufferGetBit(
    IoBuffer *IoBuf,
    void *In
    )
{
    int status = 0;
    bool *in = (bool *) In;

    assert(IoBuf != NULL);
    assert(IoBuf->Config.GetByte != NULL);
    assert(IoBuf->In.BitOffset < LAST);

    if (in == NULL)
    {
        status = ENV_BADPTR;
        WmWarnx("NULL destination pointer");
        goto Bail;
    }

    if (IoBuf->Eof)
    {
        *in = 0;
        goto Bail;
    }

    //
    // If the input buffer is empty then read in a byte.
    //

    if (IoBuf->In.BitOffset == INITIAL)
    {
        status = IoBuf->Config.GetByte(IoBuf->Config.GetContext);

        if (FAILED(status))
        {
            //
            // According to the ELVM blurb for GETC, we should read a 0 for EOF
            //
            *in = 0;

            if (!IoBuf->Eof)
            {
                IoBuf->Eof = true;
                WmWarnx("EOF (In)");
            }

            //
            // N.B., elvm expects to keep running even after EOF is received.
            //
            //status = ENV_EOF;
            status = 0;
            goto Bail;
        }
        else
        {
            IoBuf->In.Buffer = status & 0xff;
        }

    }

    *in = EXTRACT_BIT(IoBuf->In.Buffer, IoBuf->In.BitOffset++);
    if (IoBuf->In.BitOffset == LAST)
    {
        memset(&IoBuf->In, 0, sizeof(IoBuf->In));
    }

    status = 0;

Bail:
    return status;
}

int
IoBufferPutBit(
    IoBuffer *IoBuf,
    int Out
    )
{
    int status = 0;

    assert(IoBuf != NULL);
    assert(IoBuf->Config.PutByte != NULL);
    assert(IoBuf->Out.BitOffset < LAST);

    IoBuf->Out.Buffer |= EMPLACE_BIT(Out, IoBuf->Out.BitOffset++);

    //
    // If the output buffer is full then write out a byte.
    //

    if (IoBuf->Out.BitOffset == LAST)
    {
        status = IoBuf->Config.PutByte(IoBuf->Out.Buffer, IoBuf->Config.PutContext);

        memset(&IoBuf->Out, 0, sizeof(IoBuf->Out));

        if (FAILED(status))
        {
            if (!IoBuf->Eof)
            {
                WmWarnx("EOF (Out)");
                IoBuf->Eof = true;
            }

            //
            // N.B., elvm expects to keep running even after EOF is received.
            //
            //status = ENV_EOF;
            status = 0;
            goto Bail;
        }
    }

    status = 0;

Bail:
    return status;
}


#if 0
int
main(
    void
    )
{
    IoBuffer io;
    char *str = "This is a real string.\n";
    char in[1024];
    uint32_t len, i;
    int status = 0;

    IoBufferInit(&io, (GetByteFn) getc, stdin, (PutByteFn) putc, stdout); 

    len = strlen(str);
    for (i = 0; i < len; ++i)
    {
        uint32_t j, byte;

        byte = str[i];
        for (j = 0; j < 8; ++j)
        {
            status = IoBufferPutBit(&io, byte & (1 << j));
            if (FAILED(status))
            {
                printf("bad juju for put: %x\n", status);
                goto Bail;
            }
        }
    }

    for (i = 0; i < sizeof(in) - 1; ++i)
    {
        uint32_t j, byte;

        byte = 0;
        for (j = 0; j < 8; ++j)
        {
            bool bit;
            status = IoBufferGetBit(&io, &bit);
            if (FAILED(status))
            {
                printf("shiver in eternal darkness for get: %x\n", status);
                goto Bail;
            }
            byte |= bit << j;
        }
        in[i] = byte;

        if (in[i] == '\n')
        {
            break;
        }
    }
    in[i + 1] = '\0';
    printf("%s", in);

Bail:
    return (status != 0);
}
#endif
