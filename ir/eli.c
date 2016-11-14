#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ir/ir.h>

#ifdef __eir__
#define MEMSZ 0x100000
#else
#define MEMSZ 0x1000000
#endif

int pc;
Inst* prog[65536];
int mem[MEMSZ];
int regs[6];
bool verbose;

#ifdef __GNUC__
__attribute__((noreturn))
#endif
static void error(const char* msg) {
  fprintf(stderr, "%s (pc=%d)\n", msg, pc);
  exit(1);
}

static inline void dump_regs(Inst* inst) {
  bool had_negative = false;
  static const char* REG_NAMES[] = {
    "A", "B", "C", "D", "BP", "SP"
  };
  fprintf(stderr, "PC=%d ", inst->lineno);
  for (int i = 0; i < 6; i++) {
    if (regs[i] < 0)
      had_negative = true;
    fprintf(stderr, "%s=%d", REG_NAMES[i], regs[i]);
    fprintf(stderr, i == 5 ? "\n" : " ");
  }
  if (had_negative) {
    error("had negative!");
  }
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
#if defined(NOFILE) || defined(__eir__)
  Module* m = load_eir(stdin);
#else
  if (argc >= 2 && !strcmp(argv[1], "-v")) {
    verbose = true;
    argc--;
    argv++;
  }

  if (argc < 2) {
    fprintf(stderr, "no input file\n");
    return 1;
  }

  Module* m = load_eir_from_file(argv[1]);
#endif

  int i;
  i = 0;
  for (Data* d = m->data; d; d = d->next, i++) {
    mem[i] = d->v;
  }
  for (Inst* inst = m->text; inst; i++) {
    pc = inst->pc;
    prog[pc] = inst;
    for (; inst && pc == inst->pc; inst = inst->next) {}
  }

  pc = m->text->pc;
  for (;;) {
    Inst* inst = prog[pc];
    for (; inst; inst = inst->next) {
      if (verbose) {
        dump_regs(inst);
        dump_inst(inst);
      }
      int npc = -1;
      switch (inst->op) {
        case MOV:
          assert(inst->dst.type == REG);
          regs[inst->dst.reg] = src(inst);
          break;

        case ADD:
          assert(inst->dst.type == REG);
          regs[inst->dst.reg] += src(inst);
          regs[inst->dst.reg] += MEMSZ;
          regs[inst->dst.reg] %= MEMSZ;
          break;

        case SUB:
          assert(inst->dst.type == REG);
          regs[inst->dst.reg] -= src(inst);
          regs[inst->dst.reg] += MEMSZ;
          regs[inst->dst.reg] %= MEMSZ;
          break;

        case LOAD: {
          assert(inst->dst.type == REG);
          int addr = src(inst);
          if (addr < 0)
            error("zero page load");
          regs[inst->dst.reg] = mem[addr];
          break;
        }

        case STORE: {
          assert(inst->dst.type == REG);
          int addr = src(inst);
          if (addr < 0)
            error("zero page store");
          mem[addr] = regs[inst->dst.reg];
          break;
        }

        case PUTC:
          putchar(src(inst));
          break;

        case GETC: {
          int c = getchar();
          regs[inst->dst.reg] = c == EOF ? 0 : c;
          regs[inst->dst.reg] += MEMSZ;
          regs[inst->dst.reg] %= MEMSZ;
          break;
        }

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
            npc = value(&inst->jmp);
          }
          break;

        default:
          error("oops");
      }

      if (npc != -1) {
        pc = npc;
        break;
      }
    }
  }

  return 0;
}
