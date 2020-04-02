#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "IoBuffer.h"
#include "Memory.h"
#include "Environment.h"
#include "ProgramCtx.h"

#include <termios.h>


void
DisableBuffering(
    void
    )
{
    struct termios term;

    //
    // Disable all buffering
    //

    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    tcgetattr(0, &term);
    term.c_lflag &= ~ICANON;
    tcsetattr(0, TCSANOW, &term);
}

int
main(
    void
    )
{
    int status;
    Cell *rawMemory;
    IoBufferConfig io;
    Environment env;

    status = 0;

    rawMemory = malloc(MEMORY_SIZE);
    if (rawMemory == NULL)
    {
        status = 1;
        WmWarn("Unable to allocate memory");
        goto Bail;
    }
    memset(rawMemory, 0, MEMORY_SIZE);

    io.GetByte = (GetByteFn) getc;
    io.GetContext = stdin;
    io.PutByte = (PutByteFn) putc;
    io.PutContext = stdout;
    DisableBuffering();

    CHECK(status = EnvironmentInit(&env, &io, rawMemory, MEMORY_SIZE));

    Program(&env);
#if defined(PRINT_STATE_AT_EXIT)
    {
        WmPrint("\n========\nFinal state:\n");
        MemAccess(&env.memory);
        DumpMemory(&env.memory);
        WmPrint("\n========\n\n");
    }
#endif

Bail:
    return (status != 0);
}
