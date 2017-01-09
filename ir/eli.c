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
bool fast;
bool trace;
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
  if (inst)
    fprintf(stderr, "PC=%d(%s) ", inst->lineno, inst->func);
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
  while (1) {
    if (argc >= 2 && !strcmp(argv[1], "-v")) {
      verbose = true;
      argc--;
      argv++;
      continue;
    }
    if (argc >= 2 && !strcmp(argv[1], "-t")) {
      trace = true;
      argc--;
      argv++;
      continue;
    }
    if (argc >= 2 && !strcmp(argv[1], "-f")) {
      handle_call_ret();
      handle_logic_ops();
      fast = true;
      argc--;
      argv++;
      continue;
    }
    break;
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

  int max_pc = pc;
  pc = m->text->pc;
  const char* prev_func = 0;
  for (;;) {
    if (pc > max_pc) {
      dump_regs(NULL);
      error("PC overflow");
    }
    Inst* inst = prog[pc];
    for (; inst; inst = inst->next) {
      if (verbose) {
        dump_regs(inst);
        dump_inst(inst);
      }
      if (trace && prev_func != inst->func) {
        prev_func = inst->func;
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

        case AND:
          assert(fast);
          assert(inst->dst.type == REG);
          regs[inst->dst.reg] &= src(inst);
          break;

        case OR:
          assert(fast);
          assert(inst->dst.type == REG);
          regs[inst->dst.reg] |= src(inst);
          break;

        case XOR:
          assert(fast);
          assert(inst->dst.type == REG);
          regs[inst->dst.reg] ^= src(inst);
          break;

        case SLL:
          assert(fast);
          assert(inst->dst.type == REG);
          regs[inst->dst.reg] <<= src(inst);
          break;

        case SRL:
          assert(fast);
          assert(inst->dst.type == REG);
          regs[inst->dst.reg] >>= src(inst);
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

        case CALL: {
          npc = value(&inst->jmp);
          const char* fname = prog[npc]->func;
          int sp = regs[SP];
          if (!strcmp(fname, "__elvm_builtin_mul")) {
            regs[A] = mem[sp] * mem[sp + 1];
            npc = inst->pc + 1;
          } else if (!strcmp(fname, "__elvm_builtin_div")) {
            regs[A] = mem[sp] / mem[sp + 1];
            npc = inst->pc + 1;
          } else if (!strcmp(fname, "__elvm_builtin_mod")) {
            regs[A] = mem[sp] % mem[sp + 1];
            npc = inst->pc + 1;
          } else {
            regs[SP] = (regs[SP] + MEMSZ - 1) % MEMSZ;
            mem[regs[SP]] = inst->pc + 1;
          }
          break;
        }

        case RET:
          npc = mem[regs[SP]];
          regs[SP] = (regs[SP] + 1) % MEMSZ;
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
