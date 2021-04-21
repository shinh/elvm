# ELVM QFTASM (Conway's Game of Life) Backend

This directory contains several tools and examples for the QFTASM (Conway's Game of Life) Backend.
QFTASM is an assembly language for a 16-bit virtual computer architecture which was implemented on Conway's Game of Life, by various authors mentioned in the following Stack Exchange post. Therefore, this backend allows one to port programs written in C to Conway's Game of Life.

- Build a working game of Tetris in Conway's Game of Life, https://codegolf.stackexchange.com/questions/11880/build-a-working-game-of-tetris-in-conways-game-of-life , retrieved April 8, 2021.

A GitHub repository for The Quest For Tetris project (which QFTASM was developed under) is available at:

- https://github.com/QuestForTetris/QFT

In the [aforementioned Stack Exchange post](https://codegolf.stackexchange.com/questions/11880/build-a-working-game-of-tetris-in-conways-game-of-life), credits to the authors of the QFT project are given as the following (quoted verbatim from the Stack Exchange post without hyperlinks):

> This project is the culmination of the efforts of many users over the course of the past 1 & 1/2 years. Although the composition of the team has varied over time, the participants as of writing are the following:
>
> - PhiNotPi
> - El'endia Starman
> - K Zhang
> - Blue (Muddyfish)
> - Cows quack (Kritixi Lithos)
> - Mego
> - Quartata
>
> We would also like to extend our thanks to 7H3_H4CK3R, Conor O'Brien, and the many other users who have put effort into solving this challenge.

In this writing, the virtual computer architecture targeted for QFTASM will be referred to as the QFT Architecture.



## Usage

### Required Python Packages
To run the tests for QFTASM, the `pyparsing` package (`pyparsing>=2.3.1`) must be installed. This is since `./tools/qftasm/qftasm_pp.py` depends on this package to parse the *.qftasm assembly files. The tests were verified on Python 3.6.8.

### Compilation Instructions
The following commands will allow you to compile C code and run it in the QFTASM interpreter:

```sh
out/8cc -S -I. -Ilibc -Iout -o tmp.eir [input.c]
out/elc -qftasm tmp.eir > tmp.qftasmpp   # elc outputs code that requires post-processing
python ./tools/qftasm/qftasm_pp.py tmp.qftasmpp > [output.qftasm]
echo "input to stdin" | python ./tools/qftasm/qftasm_interpreter.py [output.qftasm]
```

The interpreter also has an option to take standard input interactively during runtime, rather than supplying it from a pipe. This is described later in the "Interpreter Options" section.

After elc compiles the *.eir assembly to *.qftasm, the code must be post-processed by `./tools/qftasm/qftasm_pp.py` in order to create the actual QFTASM assembly.　This is due to the fact that the current eir backend creates a jump table that converts the EIR program counter values to the QFTASM program counter values, and this table is created py post-processing.

When running the tests, `./tools/runqftasm.sh` automatically runs `./tools/qftasm/qftasm_pp.py` to perform the post-processing. However, when using the outputs of `./out/*.qftasm` , the post-processor `./tools/qftasm/qftasm_pp.py` must be run by hand in order to produce the final and actual QFTASM code.

### Porting to Conway's Game of Life
For details for porting QFTASM to Conway's Game of Life, please refer to [the Stack Exchange post for QFT](https://codegolf.stackexchange.com/questions/11880/build-a-working-game-of-tetris-in-conways-game-of-life) and its [GitHub repository](https://github.com/QuestForTetris/QFT). The [QFT-devkit](https://github.com/woodrush/QFT-devkit) can also be used to easily port QFTASM code to Conway's Game of Life.

### Testing
The test system in ELVM is closed within the QFTASM layer. Tests can be run with `make test-qftasm`, similarly as in other backends.

Some tests in `./tests` are left out at test time. This is mainly due to the fact that QFTASM is 16-bit, while ELVM is 24-bit. The list and the reasons of the filtered tests as of now are the following:

- `$(addsuffix .qftasm,$(filter out/24_%.c.eir,$(OUT.eir)))` : Involves 24-bit code
- `out/eof.c.eir.qftasm` : Involves 24-bit integers (must output 16777215 in the first line of the output)
- `out/neg.c.eir.qftasm` : Program length is larger than 1 << 16
- `out/8cc.c.eir.qftasm` : Program length is larger than 1 << 16
- `out/elc.c.eir.qftasm` : Program length is larger than 1 << 16
- `out/dump_ir.c.eir.qftasm` : Involves 24-bit integers
- `out/eli.c.eir.qftasm` : Memory pointer overflow occurs during memory initialization


## Technical Details

### Compilation Configurations
In `./target/qftasm.c` and `./tools/qftasm/qftasm_interpreter.py`, there are five configuration parameters that can be used to alter the output code:

```c
#define QFTASM_RAM_AS_STDIN_BUFFER
#define QFTASM_RAM_AS_STDOUT_BUFFER
#define QFTASM_JMPTABLE_IN_ROM
static const int QFTASM_RAMSTDIN_BUF_STARTPOSITION = 7167;
static const int QFTASM_RAMSTDOUT_BUF_STARTPOSITION = 8191;
static const int QFTASM_MEM_OFFSET = 2048
```

`QFTASM_RAM_AS_STDIN_BUFFER` and `QFTASM_RAM_AS_STDOUT_BUFFER` determines the method for handling standard input and output. By default, the compiler handles the code so that the stdin and stdout buffers are all included in the QFTASM RAM. The corresponding buffer positions are specified by `QFTASM_RAMSTDIN_BUF_STARTPOSITION` and `QFTASM_RAMSTDOUT_BUF_STARTPOSITION` . These buffers proceed backwards within the memory as the buffer length grows. Details on the implementation of standard input and output is described later in the Technical Details section.

After editing the stdio buffer options, the same parameters must be applied for the interpreter, `./tools/qftasm/qftasm_interpreter.py` . The corresponding source code is:

```python
QFTASM_RAM_AS_STDIN_BUFFER = True
QFTASM_RAM_AS_STDOUT_BUFFER = True

QFTASM_RAMSTDIN_BUF_STARTPOSITION = 7167
QFTASM_RAMSTDOUT_BUF_STARTPOSITION = 8191
```

The third parameter, `QFTASM_JMPTABLE_IN_ROM`, determines where to place the jump table in. Currently, the backend creates a jump table to map the EIR program counter values to QFTASM program counter values. By default, the jump table is created inside the ROM, expressed as consecutive jump operations near the beginning of the program. When this option is unset, the jump table will be created inside the RAM, which will save some ROM space (specifically, the number of EIR program counters) and will use up the same number of saved space in the RAM instead. This option could be used in cases when there is too few ROM space for the program while there is lots of free space available in the RAM. Since this parameter does not affect the interpreter, the interpreter can be left in place after changing this parameter.

As of now, all combinations of the `def/ndef`s of the preprocessor identifiers, `QFTASM_RAM_AS_STDIN_BUFFER`, `QFTASM_RAM_AS_STDOUT_BUFFER`, and `QFTASM_JMPTABLE_IN_ROM`, have been tested. Please run the tests for these alternate options manually when running the tests through Make.

The last parameter, `QFTASM_MEM_OFFSET`, determines the offset used to convert ELI memory addresses to QFT memory addresses. Since QFT has a unified memory mapping scheme for both registers and the memory, the backend uses a memory address offset to map between these addresses. `QFTASM_MEM_OFFSET` specifies the value of this offset. Details on memory handling is also explained later in the "Access to Native QFTASM RAM Addresses" section.

There is one more option, `#define QFTASM_SUPPRESS_MEMORY_INIT_OVERFLOW_ERROR`. While ELVM is 24-bit, the QFT architecture is 16-bit. Therefore, memory initialization for large ELVM programs may fail when ported to QFT. The `QFTASM_SUPPRESS_MEMORY_INIT_OVERFLOW_ERROR` option suppresses such errors at compile time. This option mainly exists for compatibility for the current ELVM Make system, which compiles all of the code in `./test/` before filtering them out at test time.


### Interpreter Configurations

There are also several options for the interpreter `./tools/qftasm_interpreter.py`:

```python
debug_ramdump = True
debug_plot_memdist = False   # Requires numpy and matplotlib when set to True
use_stdio = True
stdin_from_pipe = True
```

The first option `debug_ramdump` outputs a ram dump after the program terminates. The ram dump shows each RAM value as well as the number of times the RAM value was written. For simplicity, the values up to the maximum address that has a nonzero write count is shown.

The second option `debug_plot_memdist`, when set to `True`, outputs a plot of the memory usage distribution as the filename `memdist.png`. This can be used to debug memory overflow issues. Using this option requires additional installation of the `numpy` and `matplotlib` Python packages.

The third option `use_stdio`, when set to `False`, disables all standard input and output features. This can be used to explicitly specify a standalone environment using initially cleared out RAM values and only with the ROM.

The fourth option `stdin_from_pipe`, when set to `False`, allows the interpreter to wait and prompt when requiring stdin, rather than taking the entire stdin from the pipe which is the default behavior. This option can be used to simulate interactive programs which require altering the RAM during the program's runtime.


### Standard Input and Output

Since Conway's Game of Life does not have a prior definition of standard input and output, one must implement the concept manually. Therefore, there is a lot of degrees of freedom in the implementation of stdio. In the original QFTASM implementation in the aforementioned Stack Exchange post, input was handled by directly editing specific cells in the RAM during runtime. Following this concept, this backend supports two modes for stdio:

- The entire stdio buffer is put inside the RAM.
- The stdio buffer is pushed and popped through registers holding a single byte of input and output.

By default, the backend uses the first mode for handling stdio. This is especially suitable for standalone Game of Life patterns, where the input is given entirely beforehand, and the time evolution of the pattern could be calculated without any interventions afterwards during runtime.

The second mode is perhaps more suitable for an interactive use case. In this mode, the interpreter constantly observes the value of two registers after each program cycle. The compiled program writes a flag value to the stdio registers when a `PUTC` or `GETC` EIR instruction is executed. Specifically,

- `PUTC` : By default, the stdout register (at address `2`) holds the value `QFTASM_STDIO_CLOSED` (`== (1 << 9)`) from the beginning (the header part) of the program. When a `PUTC` instruction appears, the stdout value is written in the stdout register, and then the `QFTASM_STDIO_CLOSED` value becomes overwritten in the next QFTASM instruction. The interpreter observes for the changes in these values to interpret the stdout buffer.
- `GETC` : The output happens in four steps. By default, the stdin register (at address `1`) holds the value `QFTASM_STDIO_CLOSED` from the beginning (the header part) of the program. When a `GETC` instuction appears, the program first overwrites the stdin register to the value `QFTASM_STDIO_OPEN` (`== (1 << 8)`) , followed by a no-op instruction `MNZ 0 0 0` . The interpreter detects these changes of values, and is expected to write the memory value during the no-op. The no-op insruction is required due to the fact that the QFT architecture writes the resulting values of the *previous instruction* in every given program counter, rather than the instruction at the program counter. This is described in detail in the aforementioned Stack Exchange post. Therefore, the update to `QFTASM_STDIO_OPEN` actually occurs when the no-op instruction is being loaded, thus the need of the no-op. After the no-op instruction, the compiled program stores the value in the stdin register to the register specified by the `GETC` instruction, and overwrites the stdin register to `QFTASM_STDIO_CLOSED` .

The current backend admits access to native QFTASM RAM addresses in the C frontend. Therefore, a custom stdio handler can also be implemented in the C layer. This is illustrated in `./tools/qftasm/samples/calc.c` , and is also described in the following section.


### Access to Native QFTASM RAM Addresses

`./tools/qftasm/samples/calc.c` uses the following definitions to access RAM using native QFT addresses:

```c
#define QFTASM_MEM_OFFSET 95
#define QFTASM_NATIVE_ADDR(x) (x - QFTASM_MEM_OFFSET)

#define STDIN_BUF_POINTER_REG ((char*) QFTASM_NATIVE_ADDR(1))
#define curchar() (*((char*) QFTASM_NATIVE_ADDR(*STDIN_BUF_POINTER_REG)))
```

The value `QFTASM_MEM_OFFSET` is taken from the compiler configurations with the same identifier. Since the QFT architecture has a unified memory mapping scheme for both registers and memory, the ELVM backend uses the ELI address values as a virtual address with an offset `QFTASM_MEM_OFFSET` in the QFT memory address space. The `QFTASM_NATIVE_ADDR(x)` macro shown above cancels away this offset which allows access to raw QFT address values. In this example, a pointer to the register holding the current pointer of the stdin value is given as the macro `STDIN_BUF_POINTER_REG`, and the corresponding character is given as the macro `curchar()`. Such access to native QFT memory addresses can be doen naturally in the C layer as shown in this example.


### Sample Code
`./tools/qftasm/samples/calc.c` is a sample program written for showing various features of the ELVM QFTASM backend. In the header, it uses 8cc-specific manual definitions of some functions such as `malloc`. This inhibits compilation by gcc, so it is left out from the `tests` directory. When compiled with the compilation configurations shown later, this program becomes 1001 QFTASM instructions long and uses 128 bytes of QFTASM RAM at runtime, therefore allowing execution on a 10-bit-ROM, 7-bit-RAM QFT Architecture such as [Tetris.mc](https://github.com/QuestForTetris/QFT/blob/master/Tetris.mc) published in the QFT GitHub repository (https://github.com/QuestForTetris/QFT).

Since the current 8cc implementation merges unused header functions into the final output, including large headers such as `stdio.h` inflate the size of the final output QFTASM code. Therefore, to solve this problem, some usually header-related functions such as `malloc` are either manually included or implemented at the header of the program. Otherwise, the program consists of regular C code.

To reduce the RAM usage size, please specify the following compiler configurations in `./target/qftasm.c`:

```c
#define QFTASM_RAM_AS_STDIN_BUFFER
#define QFTASM_RAM_AS_STDOUT_BUFFER
#define QFTASM_JMPTABLE_IN_ROM
static const int QFTASM_RAMSTDIN_BUF_STARTPOSITION = 125;
static const int QFTASM_RAMSTDOUT_BUF_STARTPOSITION = 127;
static const int QFTASM_MEM_OFFSET = 95;
```

Also, please change the sixth line of `calc.h` to the following in alignment to the compiler configurations:

```c
#define QFTASM_MEM_OFFSET 95
```

When compiling `calc.c`, the option `-Itools/qftasm/samples` must be set to let the compiler be aware of the path for `calc.h`, as follows:

```sh
out/8cc -S -Itools/qftasm/samples -I. -Ilibc -Iout -o tmp.eir ./tools/qftasm/samples/calc.c
```

This program is a calculator program capable of calculating arithmetic expressions consisting of addition, multiplication and parentheses. It can take short (due to the limited RAM size) arithmetic expressions without whitespace, such as:

```
((1*1)+2)*2
(1+3)*2+1
(1+3)*(1+1)
1+1
5
```

and so on. Due to the limited RAM size, long or complex expressions can lead to runtime errors due to memory overflow. Otherwise, this program can interpret any arithmetic expressions of the aforementioned form.

The output of the program is placed on address `127` as a single ascii-encoded character. Therefore, proper output is limited to a single-decimal-digit number, although greater values can be inferred from the ascii output.
