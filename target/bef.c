#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <ir/ir.h>
#include <target/util.h>

static const uint BEF_MEM = 4782969;  // 9**7

typedef struct {
  char block[299][80];
  uint x;
  uint y;
  uint vx;
  bool was_jmp;
} Befunge;

Befunge g_bef;

static void bef_emit(uint c);

static void bef_clear_block_line(int y) {
  assert(y < 297);
  for (uint i = 0; i < 78; i++)
    g_bef.block[y][i] = ' ';
  g_bef.block[y][79] = '\0';
}

static void bef_block_init() {
  if (g_bef.x) {
    for (uint i = 0; i <= g_bef.y; i++) {
      puts(g_bef.block[i]);
    }
  }

  g_bef.x = 10;
  g_bef.y = 0;
  g_bef.vx = 1;
  g_bef.was_jmp = false;
  bef_clear_block_line(0);
  bef_emit('>');
}

static void bef_emit(uint c) {
  g_bef.block[g_bef.y][g_bef.x] = c;
  g_bef.x += g_bef.vx;
  if (g_bef.x == 10 || g_bef.x == 78) {
    g_bef.block[g_bef.y][g_bef.x] = 'v';
    g_bef.y++;
    g_bef.vx *= -1;
    bef_clear_block_line(g_bef.y);
    bef_emit(g_bef.vx == 1 ? '>' : '<');
  }
}

static void bef_emit_s(const char* s) {
  for (; *s; s++)
    bef_emit(*s);
}

static int bef_num_base9(uint v, char *c) {
  char *cbase = c;
  char b9[10];
  int b9l = 0;
  do {
    b9[b9l++] = v % 9;
    v /= 9;
  } while(v);
  for (int i = 0; i < b9l; i++) {
    if (i == 0 && b9[b9l - 1] == 1) {
      *c++ = '9';
      if (b9l == 1) return 1;
      i++;
    } else if (i != 0) {
      *c++ = '9';
      *c++ = '*';
    }
    uint n = b9[b9l - i - 1];
    if (n) {
      *c++ = '0' + n;
      if (i != 0) {
        *c++ = '+';
      }
    }
  }
  return c - cbase;
}

#ifndef BEFNUMCACHESIZE
#define BEFNUMCACHESIZE 6562
#endif
#ifndef BEFNUMMAXFACTOR
#define BEFNUMMAXFACTOR 59049
#endif
static char* befnumcache[BEFNUMCACHESIZE] = {
  "0","1","2","3","4","5","6","7","8","9",
  "19+", "29+", "39+", "49+", "59+", "69+", "79+", "89+", "99+",
};
static int bef_num_digits(uint v, char *c);
static int bef_num_factor_core(uint v, char *c) {
  int shortlen = 36;
  int incr = 1 + (v&1); // Skip even numbers for odd numbers
  for (uint i = 1 + incr; i*i <= v; i += incr) {
    if (v % i == 0) {
      char ijs[74];
      uint j = v / i;
      int ijlen = bef_num_digits(i, ijs);
      if (i == j && i > 9) {
        ijs[ijlen++] = ':';
      } else {
        ijlen += bef_num_digits(j, ijs + ijlen);
      }
      if (ijlen < shortlen) {
        shortlen = ijlen;
        memcpy(c, ijs, ijlen);
        c[ijlen] = '*';
        if (ijlen == 2) return 3;
      }
    }
  }
  return shortlen < 36 ? shortlen + 1 : 99;
}

static int bef_num_factor(uint v, char *c) {
  int shortlen = 99; // An arbitrarily large number bef_num_base9 will beat
  for (int off = 0; off < 19; off++) {
    char cs[37];
    int len = bef_num_factor_core(v + off - 9, cs);
    if (len < (off != 9 ? shortlen-2 : shortlen)) {
      if (off != 9) {
        cs[len] = off < 9 ? '9' - off : '0' + (off - 9);
        cs[len+1] = off < 9 ? '+' : '-';
        len += 2;
      }
      shortlen = len;
      memcpy(c, cs, len);
      if (len == 3) return 3;
    }
  }
  return shortlen;
}

static int bef_num_digits(uint v, char *c) {
  if (v < BEFNUMCACHESIZE) {
    char *code = befnumcache[v];
    if (code != NULL) {
      strcpy(c, code);
      return strlen(code);
    }
  } else if (v > BEFNUMMAXFACTOR) {
    return bef_num_base9(v, c);
  }
  int f9l = bef_num_factor(v, c);
  if (0 && f9l > 3) {
    char b9[37];
    int b9l = bef_num_base9(v, b9);
    if (b9l <= f9l) {
      memcpy(c, b9, b9l);
      if (v < BEFNUMCACHESIZE) {
        befnumcache[v] = malloc(b9l+1);
        memcpy(befnumcache[v], b9, b9l);
        befnumcache[v][b9l] = 0;
      }
      return b9l;
    }
  }
  if (v < BEFNUMCACHESIZE) {
    befnumcache[v] = malloc(f9l+1);
    memcpy(befnumcache[v], c, f9l);
    befnumcache[v][f9l] = 0;
  }
  return f9l;
}

static void bef_emit_num(uint v) {
  char c[37];
  int clen = bef_num_digits(v, c);
  for (int i = 0; i < clen; i++) {
    bef_emit(c[i]);
  }
}

static void bef_emit_uint_mod() {
  bef_emit_s("88*:*:*");
}

static void bef_emit_value(Value* v) {
  if (v->type == REG) {
    bef_emit('0' + v->reg);
    bef_emit('0');
    bef_emit('g');
  } else {
    bef_emit_num(v->imm);
  }
}

static void bef_emit_src(Inst* inst) {
  bef_emit_value(&inst->src);
}

static void bef_emit_dst(Inst* inst) {
  bef_emit_value(&inst->dst);
}

static void bef_emit_store_reg(uint r) {
  bef_emit('0' + r);
  bef_emit('0');
  bef_emit('p');
}

static void bef_make_room() {
  uint r = g_bef.vx == 1 ? 79 - g_bef.x : g_bef.x - 10;
  if (r < 10) {
    uint y = g_bef.y;
    while (y == g_bef.y)
      bef_emit(' ');
  }
}

static void bef_emit_cmp(Inst* inst, int boolify) {
  uint op = normalize_cond(inst->op, false);
  if (op == JLT || op == JGE) {
    bef_emit_src(inst);
    bef_emit_dst(inst);
    op = op == JLT ? JGT : JLE;
  } else {
    bef_emit_dst(inst);
    bef_emit_src(inst);
  }
  switch (op) {
    case JEQ:
    case JNE:
      bef_emit('-');
      break;
    case JGT:
    case JLE:
      bef_emit('`');
      break;
  }
  if (op == JEQ || op == JLE) {
    bef_emit('!');
  } else if (op == JNE && boolify) {
    bef_emit_s("!!");
  }
}

static void bef_emit_jmp(Inst* inst) {
  bef_emit_cmp(inst, 0);
  bef_make_room();
  bef_emit_s("#v_v");

  int x = g_bef.x;
  int y = g_bef.y;
  bef_clear_block_line(y + 1);
  bef_clear_block_line(y + 2);

  if (g_bef.vx == 1) {
    x -= 1;
  } else {
    x += 3;
  }

  g_bef.block[y + 2][x] = '<';
  g_bef.block[y + 2][x - 1] = '$';

  x -= 2;
  g_bef.block[y + 1][x] = '<';
  g_bef.block[y + 1][6] = '^';

  g_bef.x = x - 1;
  g_bef.y = y + 2;
  g_bef.vx = -1;
}

static void bef_emit_inst(Inst* inst) {
  switch (inst->op) {
    case MOV:
      bef_emit_src(inst);
      bef_emit_store_reg(inst->dst.reg);
      break;

    case ADD:
      bef_emit_dst(inst);
      bef_emit_src(inst);
      bef_emit('+');
      bef_emit_uint_mod();
      bef_emit('%');
      bef_emit_store_reg(inst->dst.reg);
      break;

    case SUB:
      bef_emit_dst(inst);
      bef_emit_src(inst);
      bef_emit('-');
      bef_emit_uint_mod();
      bef_emit('+');
      bef_emit_uint_mod();
      bef_emit('%');
      bef_emit_store_reg(inst->dst.reg);
      break;

    case LOAD:
      bef_emit('0');
      bef_emit_src(inst);
      bef_emit_num(BEF_MEM);
      bef_emit('+');
      bef_emit('g');
      bef_emit_store_reg(inst->dst.reg);
      break;

    case STORE:
      bef_emit_dst(inst);
      bef_emit('0');
      bef_emit_src(inst);
      bef_emit_num(BEF_MEM);
      bef_emit('+');
      bef_emit('p');
      break;

    case PUTC:
      bef_emit_src(inst);
      bef_emit(',');
      break;

    case GETC: {
      bef_make_room();
      bef_emit_s("#v~v");
      g_bef.y++;
      bef_clear_block_line(g_bef.y);
      g_bef.x += g_bef.vx == 1 ? -3 : 3;
      bef_emit(g_bef.vx == 1 ? '>' : '<');
      bef_emit('0');
      bef_emit(g_bef.vx == 1 ? '>' : '<');
      // To support implementations which return -1 on EOF.
      bef_emit_s(":1+!+");
      bef_emit_store_reg(inst->dst.reg);
      break;
    }

    case EXIT:
      bef_emit('@');
      break;

    case DUMP:
      break;

    case EQ:
    case NE:
    case LT:
    case GT:
    case LE:
    case GE:
      bef_emit_cmp(inst, 1);
      bef_emit_store_reg(inst->dst.reg);
      break;

    case JEQ:
    case JNE:
    case JLT:
    case JGT:
    case JLE:
    case JGE:
      bef_emit_value(&inst->jmp);
      bef_emit_jmp(inst);
      break;

    case JMP:
      bef_emit_value(&inst->jmp);
      g_bef.was_jmp = true;
      break;

    default:
      error("oops");
  }
}

static void bef_flush_code_block() {
  if (g_bef.vx == 1) {
    g_bef.block[g_bef.y][g_bef.x] = 'v';
    g_bef.y++;
    bef_clear_block_line(g_bef.y);
    g_bef.block[g_bef.y][g_bef.x] = '<';
  }
  if (g_bef.was_jmp) {
    g_bef.block[g_bef.y][6] = '^';
  } else {
    g_bef.block[g_bef.y][10] = 'v';
  }

  memcpy(g_bef.block[0], ">:#v_$", 6);
  memcpy(g_bef.block[1], "v-1<> ", 6);
  bef_block_init();
}

static void bef_init_state(Data* data) {
  bef_block_init();
  for (uint mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      bef_emit_num(data->v);
      bef_emit('0');
      bef_emit_num(mp);
      bef_emit_num(BEF_MEM);
      bef_emit('+');
      bef_emit('p');

      if (g_bef.y > 90) {
        bef_flush_code_block();
      }
    }
  }

  for (uint i = 0; i < 6; i++) {
    bef_emit('0');
    bef_emit_store_reg(i);
  }
  // PC
  bef_emit('0');

  while (g_bef.y == 0) {
    bef_emit(' ');
  }
  g_bef.block[g_bef.y][0] = 'v';
  g_bef.block[g_bef.y][6] = '<';
}

void target_bef(Module* module) {
  bef_init_state(module->data);

  int prev_pc = -1;
  for (Inst* inst = module->text; inst; inst = inst->next) {
    if (prev_pc != inst->pc) {
      if (prev_pc == -1)
        bef_block_init();
      else
        bef_flush_code_block();
    }
    prev_pc = inst->pc;
    bef_emit_inst(inst);
  }

  bef_flush_code_block();
}
