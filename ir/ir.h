#ifndef ELVM_IR_H_
#define ELVM_IR_H_

#include <stdio.h>

#define UINT_MAX 16777215
#define UINT_MAX_STR "16777215"
#ifdef __eir__
# define MOD24(v) v
#else
# define MOD24(v) v & UINT_MAX
#endif

typedef enum {
  A, B, C, D, BP, SP
} Reg;

typedef enum {
  REG, IMM
} ValueType;

typedef enum {
  OP_UNSET = -2, OP_ERR = -1,
  MOV = 0, ADD, SUB, LOAD, STORE, PUTC, GETC, EXIT,
  JEQ = 8, JNE, JLT, JGT, JLE, JGE, JMP,
  // Optional operations follow.
  EQ = 16, NE, LT, GT, LE, GE, DUMP,
  LAST_OP
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
  int lineno;
  char* magic_comment;
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

void split_basic_block_by_mem();

void dump_inst(Inst* inst);
void dump_inst_fp(Inst* inst, FILE* fp);

#ifdef __GNUC__
#if __has_attribute(fallthrough)
#define FALLTHROUGH __attribute__((fallthrough))
#else
#define FALLTHROUGH
#endif
#else
#define FALLTHROUGH
#endif

#endif  // ELVM_IR_H_
