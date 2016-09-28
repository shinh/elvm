#include <assert.h>
#include <stdbool.h>
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

static void bef_emit_num(uint v) {
  if (v > 9 && v < 82) {
    for (uint i = 2; i < 10; i++) {
      for (uint j = i; j < 10; j++) {
        if (i * j == v) {
          bef_emit('0' + i);
          bef_emit('0' + j);
          bef_emit('*');
          return;
        }
      }
    }
  }

  char op = '+';
  uint c[9];
  uint cs = 0;
  do {
    c[cs++] = v % 9;
    v /= 9;
  } while (v);

  for (uint i = 0; i < cs; i++) {
    if (i != 0)
      bef_emit_s("9*");
    char v = c[cs - i - 1];
    if (v || cs == 1) {
      bef_emit(v + '0');
      if (i != 0)
        bef_emit(op);
    }
  }
}

static void bef_emit_uint_mod() {
  for (int i = 0; i < 8; i++)
    bef_emit('8');
  for (int i = 0; i < 7; i++)
    bef_emit('*');
}

static void bef_emit_value(Value* v) {
  if (v->type == REG) {
    bef_emit('0' + v->reg);
    bef_emit_num(0);
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

static void bef_emit_cmp(Inst* inst) {
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
      bef_emit_s("-!");
      break;
    case JNE:
      bef_emit_s("-!!");
      break;
    case JGT:
      bef_emit_s("`");
      break;
    case JLE:
      bef_emit_s("`!");
      break;
  }
}

static void bef_emit_jmp(Inst* inst) {
  bef_emit_cmp(inst);
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
      bef_emit_cmp(inst);
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
