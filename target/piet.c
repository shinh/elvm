#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include <ir/ir.h>
#include <target/util.h>

#define PIET_IMM_BASE 6
#define PIET_INIT_STACK_SIZE 65545
#define PIET_DUMP_INST 0

enum {
  PIET_PUSH,
  PIET_POP,
  PIET_ADD,
  PIET_SUB,
  PIET_MUL,
  PIET_DIV,
  PIET_MOD,
  PIET_NOT,
  PIET_GT,
  PIET_PTR,
  PIET_SWITCH,
  PIET_DUP,
  PIET_ROLL,
  PIET_INN,
  PIET_IN,
  PIET_OUTN,
  PIET_OUT,

  PIET_JMP,
  PIET_EXIT
} PietOp;

byte PIET_COLOR_TABLE[20][3] = {
  { 0x00, 0x00, 0x00 },
  { 0xff, 0xff, 0xff },

  { 0xff, 0xc0, 0xc0 },
  { 0xff, 0x00, 0x00 },
  { 0xc0, 0x00, 0x00 },

  { 0xff, 0xff, 0xc0 },
  { 0xff, 0xff, 0x00 },
  { 0xc0, 0xc0, 0x00 },

  { 0xc0, 0xff, 0xc0 },
  { 0x00, 0xff, 0x00 },
  { 0x00, 0xc0, 0x00 },

  { 0xc0, 0xff, 0xff },
  { 0x00, 0xff, 0xff },
  { 0x00, 0xc0, 0xc0 },

  { 0xc0, 0xc0, 0xff },
  { 0x00, 0x00, 0xff },
  { 0x00, 0x00, 0xc0 },

  { 0xff, 0xc0, 0xff },
  { 0xff, 0x00, 0xff },
  { 0xc0, 0x00, 0xc0 },
};

typedef struct PietInst {
  uint op;
  uint arg;
  struct PietInst* next;
} PietInst;

typedef struct PietBlock {
  PietInst* inst;
  struct PietBlock* next;
} PietBlock;

#if PIET_DUMP_INST
static void dump_piet_inst(PietInst* pi) {
  static const char* PIET_INST_NAMES[] = {
    "push",
    "pop",
    "add",
    "sub",
    "mul",
    "div",
    "mod",
    "not",
    "gt",
    "ptr",
    "switch",
    "dup",
    "roll",
    "inn",
    "in",
    "outn",
    "out",

    "jmp",
    "exit",
  };
  fprintf(stderr, "%s", PIET_INST_NAMES[pi->op]);
  if (pi->op == PIET_PUSH) {
    fprintf(stderr, " %d", pi->arg);
  }
  fprintf(stderr, "\n");
}
#endif

static void piet_emit_a(PietInst** pi, uint op, uint arg) {
  (*pi)->next = calloc(1, sizeof(PietInst));
  *pi = (*pi)->next;
  (*pi)->op = op;
  (*pi)->arg = arg;
}

static void piet_emit(PietInst** pi, uint op) {
  piet_emit_a(pi, op, 0);
}

static void piet_push_digit(PietInst** pi, uint v) {
  assert(v > 0);
  piet_emit_a(pi, PIET_PUSH, v);
}

static void piet_push(PietInst** pi, uint v) {
  if (v == 0) {
    piet_push_digit(pi, 1);
    piet_emit(pi, PIET_NOT);
    return;
  }

  uint c[16];
  uint cs = 0;
  do {
    c[cs++] = v % PIET_IMM_BASE;
    v /= PIET_IMM_BASE;
  } while (v);

  for (uint i = 0; i < cs; i++) {
    if (i != 0) {
      piet_push_digit(pi, PIET_IMM_BASE);
      piet_emit(pi, PIET_MUL);
    }
    char v = c[cs - i - 1];
    if (v) {
      piet_push_digit(pi, v);
      if (i != 0)
        piet_emit(pi, PIET_ADD);
    }
  }
}

static void piet_push_minus1(PietInst** pi) {
  piet_push(pi, 0);
  piet_push(pi, 1);
  piet_emit(pi, PIET_SUB);
}

enum {
  PIET_A = 1,
  PIET_B,
  PIET_C,
  PIET_D,
  PIET_BP,
  PIET_SP,
  PIET_MEM
};

static void piet_roll(PietInst** pi, uint depth, uint count) {
  piet_push(pi, depth);
  piet_push(pi, count);
  piet_emit(pi, PIET_ROLL);
}

static void piet_rroll(PietInst** pi, uint depth, uint count) {
  piet_push(pi, depth);
  piet_push(pi, 1);
  piet_push(pi, count + 1);
  piet_emit(pi, PIET_SUB);
  piet_emit(pi, PIET_ROLL);
}

static void piet_load(PietInst** pi, uint pos) {
  piet_rroll(pi, pos + 1, 1);
  piet_emit(pi, PIET_DUP);
  piet_roll(pi, pos + 2, 1);
}

static void piet_store_top(PietInst** pi, uint pos) {
  piet_rroll(pi, pos + 2, 1);
  piet_emit(pi, PIET_POP);
  piet_roll(pi, pos + 1, 1);
}

static void piet_push_value(PietInst** pi, Value* v, uint stk) {
  if (v->type == REG) {
    piet_load(pi, PIET_A + v->reg + stk);
  } else if (v->type == IMM) {
    piet_push(pi, v->imm % 65536);
  } else {
    error("invalid value");
  }
}

static void piet_push_dst(PietInst** pi, Inst* inst, uint stk) {
  piet_push_value(pi, &inst->dst, stk);
}

static void piet_push_src(PietInst** pi, Inst* inst, uint stk) {
  piet_push_value(pi, &inst->src, stk);
}

static void piet_uint_mod(PietInst** pi) {
  //emit_line("16777216 mod");
  piet_push(pi, 65536);
  piet_emit(pi, PIET_MOD);
}

static void piet_cmp(PietInst** pi, Inst* inst, int stk) {
  Op op = normalize_cond(inst->op, false);
  if (op == JLT) {
    op = JGT;
    piet_push_src(pi, inst, stk);
    piet_push_dst(pi, inst, stk + 1);
  } else if (op == JGE) {
    op = JLE;
    piet_push_src(pi, inst, stk);
    piet_push_dst(pi, inst, stk + 1);
  } else {
    piet_push_dst(pi, inst, stk);
    piet_push_src(pi, inst, stk + 1);
  }
  switch (op) {
  case JEQ:
    piet_emit(pi, PIET_SUB);
    piet_emit(pi, PIET_NOT);
    break;
  case JNE:
    piet_emit(pi, PIET_SUB);
    piet_emit(pi, PIET_NOT);
    piet_emit(pi, PIET_NOT);
    break;
  case JGT:
    piet_emit(pi, PIET_GT);
    break;
  case JLE:
    piet_emit(pi, PIET_GT);
    piet_emit(pi, PIET_NOT);
    break;
  default:
    error("cmp");
  }
}

static void piet_emit_inst(PietInst** pi, Inst* inst) {
  switch (inst->op) {
  case MOV:
    piet_push_src(pi, inst, 0);
    piet_store_top(pi, PIET_A + inst->dst.reg);
    break;

  case ADD:
    piet_push_dst(pi, inst, 0);
    piet_push_src(pi, inst, 1);
    piet_emit(pi, PIET_ADD);
    piet_uint_mod(pi);
    piet_store_top(pi, PIET_A + inst->dst.reg);
    break;

  case SUB:
    piet_push_dst(pi, inst, 0);
    piet_push_src(pi, inst, 1);
    piet_emit(pi, PIET_SUB);
    piet_push(pi, 65536);
    piet_emit(pi, PIET_ADD);
    piet_uint_mod(pi);
    piet_store_top(pi, PIET_A + inst->dst.reg);
    break;

  case LOAD:
    piet_push_src(pi, inst, 0);
    piet_emit(pi, PIET_DUP);
    // Put the address to the bottom of the stack.
    piet_push(pi, PIET_INIT_STACK_SIZE);
    piet_push(pi, 1);
    piet_emit(pi, PIET_ROLL);

    piet_push(pi, PIET_MEM + 1);
    piet_emit(pi, PIET_ADD);
    piet_push_minus1(pi);
    piet_emit(pi, PIET_ROLL);
    piet_emit(pi, PIET_DUP);

    // Get the address from the bottom of the stack.
    piet_push(pi, PIET_INIT_STACK_SIZE);
    piet_push_minus1(pi);
    piet_emit(pi, PIET_ROLL);

    piet_push(pi, PIET_MEM + 2);
    piet_emit(pi, PIET_ADD);
    piet_push(pi, 1);
    piet_emit(pi, PIET_ROLL);

    piet_store_top(pi, PIET_A + inst->dst.reg);
    break;

  case STORE:
    piet_push_dst(pi, inst, 0);
    piet_push_src(pi, inst, 1);
    piet_emit(pi, PIET_DUP);

    piet_push(pi, PIET_MEM + 3);
    piet_emit(pi, PIET_ADD);
    piet_push_minus1(pi);
    piet_emit(pi, PIET_ROLL);
    piet_emit(pi, PIET_POP);

    piet_push(pi, PIET_MEM + 1);
    piet_emit(pi, PIET_ADD);
    piet_push(pi, 1);
    piet_emit(pi, PIET_ROLL);
    break;

  case PUTC:
    piet_push_src(pi, inst, 0);
    piet_emit(pi, PIET_OUT);
    break;

  case GETC: {
    // TODO: Handle EOF
#if 0
    piet_push(pi, 256);
#endif
    piet_emit(pi, PIET_IN);
#if 0
    piet_emit(pi, PIET_DUP);
    piet_push(pi, 256);
    piet_emit(pi, PIET_SUB);

    uint zero_id = piet_gen_label();
    uint done_id = piet_gen_label();

    piet_bz(zero_id);
    piet_roll(pi, 2, 1);
    piet_emit(pi, PIET_POP);
    piet_br(done_id);

    piet_label(zero_id);
    piet_emit(pi, PIET_POP);
    piet_push(pi, 0);

    piet_label(done_id);
#endif
    piet_store_top(pi, PIET_A + inst->dst.reg);

    break;
  }

  case EXIT:
    piet_emit(pi, PIET_EXIT);
    break;

  case DUMP:
    break;

  case EQ:
  case NE:
  case LT:
  case GT:
  case LE:
  case GE:
    piet_cmp(pi, inst, 0);
    piet_store_top(pi, PIET_A + inst->dst.reg);
    break;

  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
    piet_push(pi, inst->pc + 1);
    piet_push_value(pi, &inst->jmp, 1);
    piet_push(pi, 2);
    piet_cmp(pi, inst, 3);
    piet_emit(pi, PIET_ROLL);
    piet_emit(pi, PIET_POP);
    piet_emit(pi, PIET_JMP);
    break;

  case JMP:
    piet_push_value(pi, &inst->jmp, 0);
    piet_emit(pi, PIET_JMP);
    break;

  default:
    error("oops");
  }
}

static uint piet_next_color(uint c, uint op) {
  op++;
  uint l = (c + op) % 3;
  uint h = (c / 3 + op / 3) % 6;
  return l + h * 3;
}

static uint piet_init_state(Data* data, PietInst* pi) {
  uint vals[PIET_INIT_STACK_SIZE];
  for (uint i = 0; i < PIET_INIT_STACK_SIZE; i++) {
    uint v = 0;
    if (i >= PIET_MEM + 1 && data) {
      v = data->v % 65536;
      data = data->next;
    }
    vals[PIET_INIT_STACK_SIZE-i-1] = v;
  }

  PietInst* opi = pi;
  bool prev_zero = false;
  for (uint i = 0; i < PIET_INIT_STACK_SIZE; i++) {
    uint v = vals[i];
    if (v == 0 && prev_zero) {
      piet_emit(&pi, PIET_DUP);
    } else {
      piet_push(&pi, v);
    }
    prev_zero = v == 0;
  }

  uint size = 0;
  for (pi = opi->next; pi; pi = pi->next) {
    if (pi->op == PIET_PUSH) {
      size += pi->arg;
    } else {
      size++;
    }
  }
  return size;
}


void target_piet(Module* module) {
  PietBlock pb_head;
  PietBlock* pb = &pb_head;
  PietInst* pi = 0;

  int prev_pc = -1;
  for (Inst* inst = module->text; inst; inst = inst->next) {
    if (prev_pc != inst->pc) {
      if (pi && pi->op != PIET_JMP) {
        piet_push(&pi, inst->pc);
      }

      pb->next = calloc(1, sizeof(PietBlock));
      pb = pb->next;

      pi = calloc(1, sizeof(PietInst));
      pb->inst = pi;

      piet_emit(&pi, PIET_POP);

      // Dump PC.
#if 0
      piet_push(&pi, 80); piet_emit(&pi, PIET_OUT);
      piet_push(&pi, 67); piet_emit(&pi, PIET_OUT);
      piet_push(&pi, 61); piet_emit(&pi, PIET_OUT);
      piet_push(&pi, inst->pc); piet_emit(&pi, PIET_OUTN);
      piet_push(&pi, 10); piet_emit(&pi, PIET_OUT);
#endif
    }
    prev_pc = inst->pc;
    piet_emit_inst(&pi, inst);
  }

  int pc = 0;
  int longest_block = 0;
  for (pb = pb_head.next; pb; pb = pb->next) {
    pc++;
    int block_len = 0;
    for (pi = pb->inst->next; pi; pi = pi->next) {
      block_len++;
    }
    if (longest_block < block_len)
      longest_block = block_len;
  }

#if PIET_DUMP_INST
  pc = 0;
  for (pb = pb_head.next; pb; pb = pb->next) {
    fprintf(stderr, "\npc=%d:\n", pc++);
    for (pi = pb->inst->next; pi; pi = pi->next) {
      fprintf(stderr, " ");
      dump_piet_inst(pi);
    }
  }
#endif

  PietInst init_state = {};
  uint init_state_size = piet_init_state(module->data, &init_state);

  uint w = longest_block + 20;
  uint h =
      pc * 7 + (init_state_size / (w - 4 - PIET_IMM_BASE * 2) + 1) * 4 + 10;
  byte* pixels = calloc(w * h, 1);

#if 0
  fprintf(stderr, "estimated init_state_size=%d w=%d h=%d\n",
          init_state_size, w, h);
#endif

  uint c = 0;
  uint y = 0;
  uint x = 0;
  int dx = 1;
  pixels[y*w+x++] = 2;
  for (pi = init_state.next; pi; pi = pi->next) {
    assert(y < h);
    c = piet_next_color(c, pi->op);

    if (pi->next && pi->next->op == PIET_PUSH) {
      for (uint i = 0; i < pi->next->arg; i++) {
        pixels[y*w+x] = c + 2;
        x += dx;
      }
    } else {
      pixels[y*w+x] = c + 2;
      x += dx;
    }

    if (x >= w - PIET_IMM_BASE && dx == 1) {
      pixels[(y+1)*w+x-1] = c + 2;
      pixels[(y+2)*w+x-1] = c + 2;
      pixels[(y+3)*w+x-1] = c + 2;
      x = x-1;
      y += 4;
      dx = -1;
    }

    if ((x <= 1 + PIET_IMM_BASE || !pi->next) && dx == -1) {
      while (x >= w - 2) {
        pixels[(y+0)*w+x+0] = 1;
        x--;
      }
      pixels[(y+0)*w+x+0] = 1;
      pixels[(y+0)*w+x-1] = 1;
      pixels[(y-1)*w+x-1] = 1;
      pixels[(y-1)*w+x+0] = 1;
      pixels[(y+1)*w+x+0] = 1;
      pixels[(y+2)*w+x+0] = 1;
      pixels[(y+3)*w+x+0] = 1;
      pixels[(y+3)*w+x-1] = 1;
      pixels[(y+2)*w+x-1] = 1;
      pixels[(y+2)*w+x+1] = 2;
      c = 0;
      x += 2;
      y += 2;
      dx = 1;
    }
  }

  for (; x < w; x++) {
    pixels[y*w+x] = 1;
  }

  pixels[(y+1)*w+w-1] = 1;
  pixels[(y+2)*w+w-1] = 1;

  y += 3;

  c = 0;
  byte BORDER_TABLE[7];
  BORDER_TABLE[0] = 1;
  BORDER_TABLE[1] = 2;
  c = piet_next_color(c, PIET_PUSH);
  BORDER_TABLE[2] = c + 2;
  c = piet_next_color(c, PIET_SUB);
  BORDER_TABLE[3] = c + 2;
  c = piet_next_color(c, PIET_DUP);
  BORDER_TABLE[4] = c + 2;
  c = piet_next_color(c, PIET_NOT);
  BORDER_TABLE[5] = c + 2;
  c = piet_next_color(c, PIET_PTR);
  BORDER_TABLE[6] = c + 2;

  for (uint i = 0; i < 3; i++) {
    pixels[(y+i)*w+w-1] = 1;
  }

  for (uint x = 0; x < w; x++) {
    pixels[(y+3)*w+x] = 1;
  }

  for (uint i = 3; i < h - y; i++) {
    pixels[(y+i)*w] = 1;
    pixels[(y+i)*w+w-1] = BORDER_TABLE[i%7];
  }

  y += 6;

  for (pb = pb_head.next; pb; pb = pb->next, y += 7) {
    assert(y < h);
    pixels[y*w+w-2] = 1;
    pixels[y*w+w-3] = 2;
    uint x = w - 3;
    c = 0;
    bool goto_next = true;
    for (pi = pb->inst->next; pi; pi = pi->next, x--) {
      assert(x < w);
      if (pi->op == PIET_PUSH) {
        assert(pi->arg);
        for (uint i = 0; i < pi->arg; i++) {
          pixels[(y+i)*w+x] = c + 2;
        }
      } else if (pi->op == PIET_JMP) {
        break;
      } else if (pi->op == PIET_EXIT) {
        pixels[(y+1)*w+x] = pixels[(y+0)*w+x];
        pixels[(y+1)*w+x-1] = 1;
        pixels[(y+0)*w+x-2] = 3;
        pixels[(y+1)*w+x-2] = 3;
        pixels[(y+2)*w+x-2] = 3;
        goto_next = false;
        break;
      }

      if (!goto_next)
        break;

      c = piet_next_color(c, pi->op);
      pixels[y*w+x-1] = c + 2;
    }

    if (goto_next) {
      for (x--; x > 0; x--) {
        pixels[y*w+x] = 1;
      }
    }
  }

  h = y + 10;

  printf("P6\n");
  printf("#\n");
  printf("%d %d\n", w, h);
  printf("255\n");

  for (uint y = 0; y < h; y++) {
    for (uint x = 0; x < w; x++) {
      byte* c = PIET_COLOR_TABLE[pixels[y*w+x]];
      putchar(c[0]);
      putchar(c[1]);
      putchar(c[2]);
    }
  }
}
