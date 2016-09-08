#include "ir.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int pc;
Inst* prog[1 << 16];
int mem[1 << 24];
int regs[6];

__attribute__((noreturn))
static void error(const char* msg) {
  fprintf(stderr, "%s (pc=%d)\n", msg, pc);
  exit(1);
}

static int value(Value* v) {
  if (v->type == REG) {
    return regs[v->reg];
  } else if (v->type == IMM) {
    return v->imm;
  } else {
    error("invalid value");
  }
}

static int src(Inst* inst) {
  return value(&inst->src);
}

static int cmp(Inst* inst) {
  int op = inst->op;
  if (op >= 16)
    op -= 8;
  assert(inst->dst.type == REG);
  int d = regs[inst->dst.reg];
  int s = src(inst);
  switch (op) {
    case JEQ:
      return d == s;
    case JNE:
      return d != s;
    case JLT:
      return d < s;
    case JGT:
      return d > s;
    case JLE:
      return d <= s;
    case JGE:
      return d >= s;
    case JMP:
      return 1;
    default:
      error("oops");
  }
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    fprintf(stderr, "no input file\n");
    return 1;
  }

  Module* m = load_eir_from_file(argv[1]);
  int i;
  i = 0;
  for (Data* d = m->data; d; d = d->next, i++) {
    mem[i] = d->v;
  }
  i = 0;
  for (Inst* inst = m->text; inst; i++) {
    prog[i] = inst;
    pc = inst->pc;
    for (; inst && pc == inst->pc; inst = inst->next) {}
  }

  pc = 0;
  for (;;) {
    Inst* inst = prog[pc];
    for (; inst && inst->pc == pc; inst = inst->next) {
      switch (inst->op) {
        case MOV:
          assert(inst->dst.type == REG);
          regs[inst->dst.reg] = src(inst);
          break;

        case ADD:
          assert(inst->dst.type == REG);
          regs[inst->dst.reg] += src(inst);
          break;

        case SUB:
          assert(inst->dst.type == REG);
          regs[inst->dst.reg] -= src(inst);
          break;

        case LOAD:
          assert(inst->dst.type == REG);
          regs[inst->dst.reg] = mem[src(inst)];
          break;

        case STORE:
          assert(inst->dst.type == REG);
          mem[src(inst)] = regs[inst->dst.reg];
          break;

        case PUTC:
          putchar(src(inst));
          break;

        case GETC:
          regs[inst->dst.reg] = getchar();
          break;

        case EXIT:
          exit(0);

        case DUMP:
          break;

        case EQ:
        case NE:
        case LT:
        case GT:
        case LE:
        case GE:
          regs[inst->dst.reg] = cmp(inst);
          break;

        case JEQ:
        case JNE:
        case JLT:
        case JGT:
        case JLE:
        case JGE:
        case JMP:
          if (cmp(inst)) {
            pc = value(&inst->jmp);
          }
          break;

        default:
          error("oops");
      }
    }
  }

  return 0;
}
