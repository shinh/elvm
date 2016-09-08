#ifndef ELVM_IR_H_
#define ELVM_IR_H_

#include <stdio.h>

typedef enum {
  A, B, C, D, BP, SP
} Reg;

typedef enum {
  REG, IMM, TMP
} ValueType;

typedef enum {
  OP_UNSET = -2, OP_ERR = -1,
  MOV = 0, ADD, SUB, LOAD, STORE, PUTC, GETC, EXIT,
  JEQ = 8, JNE, JLT, JGT, JLE, JGE, JMP,
  // Optional operations follow.
  EQ = 16, NE, LT, GT, LE, GE, DUMP
} Op;

typedef struct {
  ValueType type;
  union {
    Reg reg;
    int imm;
    void* tmp;
  };
} Value;

typedef struct Inst_ {
  Op op;
  Value dst;
  Value src;
  Value jmp;
  int pc;
  struct Inst_* next;
} Inst;

typedef struct Data_ {
  int v;
  struct Data_* next;
} Data;

typedef struct {
  Inst* text;
  Data* data;
} Module;

Module* load_eir(FILE* fp);

Module* load_eir_from_file(const char* filename);

void dump_inst(Inst* inst);

#endif  // ELVM_IR_H_
