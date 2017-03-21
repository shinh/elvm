# Esoteric Language Virtual Machine

## Registers and memory

* Words are unsigned integers. The word-size is backend dependent, but
  most backends use 24-bit words.
* 6 registers: A, B, C, D, SP, and BP. They are one word wide and
  initialized to zero.
* Memory addresses are one word wide. The beginning of memory is
  initialized from the .data segments; the rest is initialized to
  zero.
* Instructions are not stored in memory. Every instruction in a basic
  block has the same pc (program counter) value, and a branch to a pc
  goes to the first instruction with that pc.

## Ops

MOV dst, src
- copy src to dst
- src: immediate or register
- dst: register

ADD dst, src
- add src to dst and places result in dst
- src: immediate or register
- dst: register
- no carry flag

SUB dst, src
- subtract src from dst and places result in dst
- src: immediate or register
- dst: register
- no borrow flag

LOAD dst, src
- copy address src to dst
- src: immediate or register
- dst: register

STORE src, dst
- copy src to address dst
- src: register
- dst: immediate or register
(but internally in struct Inst defined in ir/ir.h dst is stored in
Inst.src and src is stored in Inst.dst for implementation reason)

PUTC src
- print character (mod 256) from src
- src: immediate or register

GETC dst
- read a character, or 0 if EOF, into dst
- dst: register

EXIT
- exit the program

JEQ/JNE/JLT/JGT/JLE/JGE jmp, dst, src
- compare dst and src and, if true, jump to jmp
- src: immediate or register
- dst: register
- jmp: immediate or register

JMP jmp
- unconditionally jump to jmp
- jmp: immediate or register

EQ/NE/LT/GT/LE/GE dst, src
- compare dst and src and place result (0 or 1) into dst
- src: immediate or register
- dst: register

DUMP
- no-op

## Text format (aka .eir file)

The syntax of the text format is borrowed from GNU assembler. Please
check its manual if you are not familiar with it. [Pseudo
ops](https://sourceware.org/binutils/docs/as/Pseudo-Ops.html#Pseudo-Ops)
are especially important. Currently, .text, .data, .long, and .string
are used. And others may be ignored or cause an error.
