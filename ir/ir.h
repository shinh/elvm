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

// MEMO: instructions https://github.com/shinh/elvm/blob/master/ELVM.md#ops
typedef enum {
  OP_UNSET = -2, OP_ERR = -1,
  // MEMO: auto increみたいなあるのかw
  MOV = 0, ADD, SUB, LOAD, STORE, PUTC, GETC, EXIT,
  JEQ = 8, JNE, JLT, JGT, JLE, JGE, JMP,
  // Optional operations follow.
  EQ = 16, NE, LT, GT, LE, GE, DUMP,
  LAST_OP
} Op;

// MEMO: バリアント. https://ja.wikipedia.org/wiki/%E5%85%B1%E7%94%A8%E4%BD%93#:~:text=%E3%81%BE%E3%81%A7%E3%81%AF%E7%84%A1%E5%90%8D%E3%81%AE%E6%A7%8B%E9%80%A0%E4%BD%93%E3%81%8A%E3%82%88%E3%81%B3%E5%85%B1%E7%94%A8%E4%BD%93%E3%81%8C%E8%A8%B1%E5%8F%AF%E3%81%95%E3%82%8C%E3%81%A6%E3%81%84%E3%81%AA%E3%81%8B
// memoryかレジスタが入る
typedef struct {
  ValueType type;
  union {
    // レジスタ
    Reg reg;
    // メモリ
    int imm;
    void* tmp;
  };
} Value;

// MEMO: instructionを表す.
typedef struct Inst_ {
  Op op;
  Value dst;
  // このistructionのoperationのsrc オペランド 
  Value src;
  // このistructionのoperationのdst オペランド 
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
