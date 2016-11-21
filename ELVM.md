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

PUTC src
- print character (mod 256) from src
- src: immediate or register

GETC dst
- read a character, or 0 if EOF, into dst
- dst: register

EXIT
- exit the program

JEQ/JNE/JLT/JGT/JLE/JGE dst, src, jmp
- compare dst and src and, if true, jump to jmp

JMP jmp
- unconditionally jump to jmp

EQ/NE/LT/GT/LE/GE dst, src
- compare dst and src and place result (0 or 1) into dst
- src: immediate or register
- dst: register

DUMP
- no-op
