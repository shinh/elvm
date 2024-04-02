#include <assert.h>

#include <target/util.h>

#define AHEUI_BLOCK_HEIGHT 4000

typedef struct {
  char block[AHEUI_BLOCK_HEIGHT][80];
  uint x;
  uint y;
  uint dx;
  bool was_jmp;
} Aheui;

Aheui g_aheui;

enum aheui_op {
  AHEUI_OP_DIV = 2,
  AHEUI_OP_ADD = 3,
  AHEUI_OP_MUL = 4,
  AHEUI_OP_MOD = 5,
  AHEUI_OP_POP = 6,
  AHEUI_OP_PUSH = 7,
  AHEUI_OP_DUP = 8,
  AHEUI_OP_SELECT = 9,
  AHEUI_OP_MOVE = 10,
  AHEUI_OP_NUL = 11,
  AHEUI_OP_CMP = 12,
  AHEUI_OP_BRANCH = 14,
  AHEUI_OP_SUB = 16,
  AHEUI_OP_SWAP = 17,
  AHEUI_OP_EXIT = 18,
};

enum aheui_dir  {
  AHEUI_DIR_RIGHT = 0,
  AHEUI_DIR_RIGHT2 = 2,
  AHEUI_DIR_LEFT = 4,
  AHEUI_DIR_LEFT2 = 6,
  AHEUI_DIR_UP = 8,
  AHEUI_DIR_UP2 = 12,
  AHEUI_DIR_DOWN = 13,
  AHEUI_DIR_DOWN2 = 17,
};

static uint aheuinumtojong[10] = {
  0, -1, 4, 7, 16, 8, 18, 9, 15, 10,
};
static const uint AHEUI_MEMORY1 = 22;
static const uint AHEUI_MEMORY2 = 23;
const uint aheui_memory_step = 2; // didn't want to use 반반나

static const uint AHEUI_EMPTY_CODEPOINT = 12615; // ㅇ

static const uint AHEUI_X_CONTROL = 21;

static void aheui_emit_op(uint op, uint jong);
static void aheui_emit_utf8(char *p, uint codepoint);
static void aheui_emit_value(Value* v);
static uint aheui_syllable(uint cho, uint jung, uint jong);

static void aheui_clear_block_line(int y) {
  assert(y < AHEUI_BLOCK_HEIGHT - 2);
  for (uint i = 0; i < 78; i+=3)
    aheui_emit_utf8(&(g_aheui.block[y][i]), AHEUI_EMPTY_CODEPOINT);
    // g_aheui.block[y][i] = ' ';
  g_aheui.block[y][79] = '\0';
}

static void aheui_block_init() {
  if (g_aheui.x) {
    for (uint i = 0; i <= g_aheui.y; i++) {
      puts(g_aheui.block[i]);
    }
  }

  g_aheui.x = 0;
  g_aheui.y = 0;
  g_aheui.dx = 1;
  g_aheui.was_jmp = false;
  aheui_clear_block_line(0);
  aheui_emit_utf8(&(g_aheui.block[g_aheui.y][g_aheui.x]), aheui_syllable(AHEUI_OP_NUL, AHEUI_DIR_RIGHT, 0));
  g_aheui.x = AHEUI_X_CONTROL + 3;
}

static void aheui_emit_op(uint op, uint jong) {
  uint codepoint = AHEUI_EMPTY_CODEPOINT;
  if (op != AHEUI_OP_NUL) {
    codepoint = aheui_syllable(op, (g_aheui.dx == 1) ? AHEUI_DIR_RIGHT : AHEUI_DIR_LEFT, jong);
  }
  /*
  if (g_aheui.x == 15 || g_aheui.x == 78) {
    codepoint = aheui_syllable(op, AHEUI_DIR_DOWN, jong);
  }
  */
  aheui_emit_utf8(&(g_aheui.block[g_aheui.y][g_aheui.x]), codepoint);
  g_aheui.x += (3 * g_aheui.dx);
  if (g_aheui.x == AHEUI_X_CONTROL + 3 || g_aheui.x == 75) {
    aheui_emit_utf8(&(g_aheui.block[g_aheui.y][g_aheui.x]), aheui_syllable(AHEUI_OP_NUL, AHEUI_DIR_DOWN, 0));
    g_aheui.y++;
    aheui_clear_block_line(g_aheui.y);
    g_aheui.dx *= -1;
    aheui_emit_utf8(&(g_aheui.block[g_aheui.y][g_aheui.x]), aheui_syllable(AHEUI_OP_NUL, (g_aheui.dx == 1) ? AHEUI_DIR_RIGHT : AHEUI_DIR_LEFT, 0));
    g_aheui.x += (3 * g_aheui.dx);
  }
}

static void aheui_emit_num_base9(uint v) {
  char b9[10];
  int b9l = 0;
  if (v == 0) {
    aheui_emit_op(AHEUI_OP_PUSH, aheuinumtojong[0]);
    return;
  }
  do {
    b9[b9l++] = v % 9;
    v /= 9;
  } while(v);
  for (int i = 0; i < b9l; i++) {
    if (b9l > 1 && i == 0 && b9[b9l - 1] == 1) {
      aheui_emit_op(AHEUI_OP_PUSH, aheuinumtojong[9]);
      if (b9l == 1) return;
      i++;
    } else if (i != 0) {
      aheui_emit_op(AHEUI_OP_PUSH, aheuinumtojong[9]);
      aheui_emit_op(AHEUI_OP_MUL, 0);
    }
    uint n = b9[b9l - i - 1];
    if (n) {
      if (n == 1) {
        aheui_emit_op(AHEUI_OP_PUSH, aheuinumtojong[2]);
        aheui_emit_op(AHEUI_OP_PUSH, aheuinumtojong[2]);
        aheui_emit_op(AHEUI_OP_DIV, 0);
      } else {
        aheui_emit_op(AHEUI_OP_PUSH, aheuinumtojong[n]);
      }
      if (i != 0) {
        aheui_emit_op(AHEUI_OP_ADD, 0);
      }
    }
  }
}

static uint aheui_syllable(uint cho, uint jung, uint jong) {
  return 0xAC00 + cho * 21 * 28 + jung * 28 + jong;
}

static void aheui_emit_utf8(char *p, uint codepoint) {
  /*
  g.aheui.block[g_aheui.y][g_aheui.x] = 0xE0 + codepoint >> 12;
  g.aheui.block[g_aheui.y][g_aheui.x + 1] = 0x80 + (codepoint >> 6) % (1 << 6);
  g.aheui.block[g_aheui.y][g_aheui.x + 2] = 0x80 + codepoint % (1 << 6);
  */
  p[0] = 0xE0 + (codepoint >> 12);
  p[1] = 0x80 + (codepoint >> 6) % (1 << 6);
  p[2] = 0x80 + codepoint % (1 << 6);
}

static void aheui_emit_num(uint v) {
  aheui_emit_num_base9(v);
}

static void aheui_emit_uint_mod() {
  aheui_emit_num(8);
  aheui_emit_num(8);
  aheui_emit_op(AHEUI_OP_MUL, 0);
  aheui_emit_op(AHEUI_OP_DUP, 0);
  aheui_emit_op(AHEUI_OP_MUL, 0);
  aheui_emit_op(AHEUI_OP_DUP, 0);
  aheui_emit_op(AHEUI_OP_MUL, 0);
}

static void aheui_emit_value(Value* v) {
  if (v->type == REG) {
    aheui_emit_op(AHEUI_OP_SELECT, v->reg + 1);
    aheui_emit_op(AHEUI_OP_DUP, 0);
    aheui_emit_op(AHEUI_OP_MOVE, 0);
    aheui_emit_op(AHEUI_OP_SELECT, 0);
  } else {
    aheui_emit_num(v->imm);
  }
}

static void aheui_emit_src(Inst* inst) {
  aheui_emit_value(&inst->src);
}

static void aheui_emit_dst(Inst* inst) {
  aheui_emit_value(&inst->dst);
}

static void aheui_emit_store_reg(uint r) {
  aheui_emit_op(AHEUI_OP_MOVE, r + 1);
  aheui_emit_op(AHEUI_OP_SELECT, r + 1);
  aheui_emit_op(AHEUI_OP_SWAP, 0);
  aheui_emit_op(AHEUI_OP_POP, 0);
  aheui_emit_op(AHEUI_OP_SELECT, 0);
}

static void aheui_make_room(uint w) {
  uint r = g_aheui.dx == 1 ? 79 - g_aheui.x : g_aheui.x - AHEUI_X_CONTROL;
  if (r < 3 * (w + 2)) {
    uint y = g_aheui.y;
    while (y == g_aheui.y)
      aheui_emit_op(AHEUI_OP_NUL, 0);
  }
}

static void aheui_emit_cmp(Inst* inst, int boolify) {
  uint op = normalize_cond(inst->op, false);
  if (op == JLT || op == JGE) {
    aheui_emit_dst(inst);
    aheui_emit_src(inst);
    op = op == JLT ? JGT : JLE;
  } else {
    aheui_emit_src(inst);
    aheui_emit_dst(inst);
  }
  switch (op) {
    case JEQ:
    case JNE:
      aheui_emit_op(AHEUI_OP_SUB, 0);
      break;
    case JGT:
    case JLE:
      aheui_emit_op(AHEUI_OP_CMP, 0);
      break;
  }
  if (op == JEQ || op == JNE) {
    // 야분차부
    // ㅇ반나아
    aheui_make_room(6);
    aheui_clear_block_line(g_aheui.y + 1);

    aheui_emit_utf8(&g_aheui.block[g_aheui.y][g_aheui.x], aheui_syllable(AHEUI_OP_NUL, (g_aheui.dx == 1) ? AHEUI_DIR_RIGHT2 : AHEUI_DIR_LEFT2, 0));
    aheui_emit_utf8(&g_aheui.block[g_aheui.y][g_aheui.x + (3 * g_aheui.dx)], aheui_syllable(AHEUI_OP_PUSH, AHEUI_DIR_DOWN, aheuinumtojong[2]));
    aheui_emit_utf8(&g_aheui.block[g_aheui.y][g_aheui.x + (6 * g_aheui.dx)], aheui_syllable(AHEUI_OP_BRANCH, (g_aheui.dx == 1) ? AHEUI_DIR_RIGHT : AHEUI_DIR_LEFT, 0));
    aheui_emit_utf8(&g_aheui.block[g_aheui.y][g_aheui.x + (9 * g_aheui.dx)], aheui_syllable(AHEUI_OP_PUSH, AHEUI_DIR_DOWN, aheuinumtojong[0]));
    aheui_emit_utf8(&g_aheui.block[g_aheui.y + 1][g_aheui.x + (3 * g_aheui.dx)], aheui_syllable(AHEUI_OP_PUSH, (g_aheui.dx == 1) ? AHEUI_DIR_RIGHT : AHEUI_DIR_LEFT, aheuinumtojong[2]));
    aheui_emit_utf8(&g_aheui.block[g_aheui.y + 1][g_aheui.x + (6 * g_aheui.dx)], aheui_syllable(AHEUI_OP_DIV, (g_aheui.dx == 1) ? AHEUI_DIR_RIGHT : AHEUI_DIR_LEFT, 0));
    aheui_emit_utf8(&g_aheui.block[g_aheui.y + 1][g_aheui.x + (9 * g_aheui.dx)], aheui_syllable(AHEUI_OP_NUL, (g_aheui.dx == 1) ? AHEUI_DIR_RIGHT : AHEUI_DIR_LEFT, 0));
    g_aheui.y += 1;
    g_aheui.x += 12 * g_aheui.dx;
  }
  if (op == JNE || op == JGT) {
    // 반반나파타
    aheui_emit_num(2);
    aheui_emit_num(2);
    aheui_emit_op(AHEUI_OP_DIV, 0);
    aheui_emit_op(AHEUI_OP_SWAP, 0);
    aheui_emit_op(AHEUI_OP_SUB, 0);
  } else if (op == JNE && boolify) {
    // 반반나파자
    // aheui_emit_num(2);
    // aheui_emit_num(2);
    // aheui_emit_op(AHEUI_OP_SUB, 0);
    // aheui_emit_op(AHEUI_OP_SWAP, 0);
    // aheui_emit_op(AHEUI_OP_CMP, 0);
  }
}

static void aheui_emit_jmp(Inst* inst) {
  aheui_emit_cmp(inst, 0);
  //   #v_v
  // ^  <
  //     $<
  // ㅇㅇㅇㅇㅇㅇ야우차우
  // 오ㅇㅇㅇㅇㅇㅇㅇㅇ어
  // ㅇㅇ우ㅇㅇㅇㅇ머
  aheui_make_room(4);
  aheui_clear_block_line(g_aheui.y + 1);
  aheui_clear_block_line(g_aheui.y + 2);
  aheui_emit_utf8(&g_aheui.block[g_aheui.y][g_aheui.x], aheui_syllable(AHEUI_OP_NUL, (g_aheui.dx == 1) ? AHEUI_DIR_RIGHT2 : AHEUI_DIR_LEFT2, 0));
  aheui_emit_utf8(&g_aheui.block[g_aheui.y][g_aheui.x + (3 * g_aheui.dx)], aheui_syllable(AHEUI_OP_NUL, AHEUI_DIR_DOWN, 0));
  aheui_emit_utf8(&g_aheui.block[g_aheui.y][g_aheui.x + (6 * g_aheui.dx)], aheui_syllable(AHEUI_OP_BRANCH, (g_aheui.dx == 1) ? AHEUI_DIR_RIGHT : AHEUI_DIR_LEFT, 0));
  aheui_emit_utf8(&g_aheui.block[g_aheui.y][g_aheui.x + (9 * g_aheui.dx)], aheui_syllable(AHEUI_OP_NUL, AHEUI_DIR_DOWN, 0));

  aheui_emit_utf8(&g_aheui.block[g_aheui.y + 1][g_aheui.x + (9 * g_aheui.dx)], aheui_syllable(AHEUI_OP_NUL, AHEUI_DIR_LEFT, 0));
  aheui_emit_utf8(&g_aheui.block[g_aheui.y + 1][AHEUI_X_CONTROL], aheui_syllable(AHEUI_OP_NUL, AHEUI_DIR_UP, 0));
  aheui_emit_utf8(&g_aheui.block[g_aheui.y + 2][g_aheui.x + (3 * g_aheui.dx)], aheui_syllable(AHEUI_OP_POP, AHEUI_DIR_LEFT, 0));

  g_aheui.x += 6 * g_aheui.dx;
  g_aheui.x -= 3;
  g_aheui.y += 2;
  g_aheui.dx = -1;
}

static void aheui_emit_seek_memory(Value* pos) {
  if (pos->type == REG) {
    aheui_emit_op(AHEUI_OP_SELECT, pos->reg + 1);
    aheui_emit_op(AHEUI_OP_DUP, 0);
    aheui_emit_op(AHEUI_OP_MOVE, 0);
    aheui_emit_op(AHEUI_OP_SELECT, 0);
    aheui_emit_op(AHEUI_OP_PUSH, aheuinumtojong[aheui_memory_step]);
    aheui_emit_op(AHEUI_OP_MUL, 0);
  } else {
    aheui_emit_num(aheui_memory_step * pos->imm);
  }

  // 사빠샺쑤뻐ㅇㅇㅇㅇㅇㅇㅇ바사쑺
  // ㅇㅇㅇ사파주ㅇㅇㅇㅇㅇㅇ솢
  // 쏯썿섲ㅇㅇ차빠샂빠싸사탸뽀처마샂쌏
  aheui_make_room(15);
  aheui_clear_block_line(g_aheui.y + 1);
  aheui_clear_block_line(g_aheui.y + 2);
  aheui_emit_utf8(&g_aheui.block[g_aheui.y][g_aheui.x], aheui_syllable(AHEUI_OP_SELECT, (g_aheui.dx == 1) ? AHEUI_DIR_RIGHT : AHEUI_DIR_LEFT, 0));
  aheui_emit_utf8(&g_aheui.block[g_aheui.y][g_aheui.x + (3 * g_aheui.dx)], aheui_syllable(AHEUI_OP_DUP, (g_aheui.dx == 1) ? AHEUI_DIR_RIGHT : AHEUI_DIR_LEFT, 0));
  aheui_emit_utf8(&g_aheui.block[g_aheui.y][g_aheui.x + (6 * g_aheui.dx)], aheui_syllable(AHEUI_OP_SELECT, (g_aheui.dx == 1) ? AHEUI_DIR_RIGHT2 : AHEUI_DIR_LEFT2, AHEUI_MEMORY1));
  aheui_emit_utf8(&g_aheui.block[g_aheui.y][g_aheui.x + (9 * g_aheui.dx)], aheui_syllable(AHEUI_OP_MOVE, AHEUI_DIR_DOWN, 0));
  aheui_emit_utf8(&g_aheui.block[g_aheui.y][g_aheui.x + (12 * g_aheui.dx)], aheui_syllable(AHEUI_OP_DUP, (g_aheui.dx == 1) ? AHEUI_DIR_LEFT : AHEUI_DIR_RIGHT, 0));
  aheui_emit_utf8(&g_aheui.block[g_aheui.y][g_aheui.x + (36 * g_aheui.dx)], aheui_syllable(AHEUI_OP_PUSH, (g_aheui.dx == 1) ? AHEUI_DIR_RIGHT : AHEUI_DIR_LEFT, 0));
  aheui_emit_utf8(&g_aheui.block[g_aheui.y][g_aheui.x + (39 * g_aheui.dx)], aheui_syllable(AHEUI_OP_SELECT, (g_aheui.dx == 1) ? AHEUI_DIR_RIGHT : AHEUI_DIR_LEFT, 0));
  aheui_emit_utf8(&g_aheui.block[g_aheui.y][g_aheui.x + (42 * g_aheui.dx)], aheui_syllable(AHEUI_OP_MOVE, AHEUI_DIR_DOWN, AHEUI_MEMORY1));
  aheui_emit_utf8(&g_aheui.block[g_aheui.y + 1][g_aheui.x + (9 * g_aheui.dx)], aheui_syllable(AHEUI_OP_SELECT, (g_aheui.dx == 1) ? AHEUI_DIR_RIGHT : AHEUI_DIR_LEFT, 0));
  aheui_emit_utf8(&g_aheui.block[g_aheui.y + 1][g_aheui.x + (12 * g_aheui.dx)], aheui_syllable(AHEUI_OP_SWAP, (g_aheui.dx == 1) ? AHEUI_DIR_RIGHT : AHEUI_DIR_LEFT, 0));
  aheui_emit_utf8(&g_aheui.block[g_aheui.y + 1][g_aheui.x + (15 * g_aheui.dx)], aheui_syllable(AHEUI_OP_CMP, AHEUI_DIR_DOWN, 0));
  aheui_emit_utf8(&g_aheui.block[g_aheui.y + 1][g_aheui.x + (36 * g_aheui.dx)], aheui_syllable(AHEUI_OP_SELECT, AHEUI_DIR_UP, AHEUI_MEMORY1));
  aheui_emit_utf8(&g_aheui.block[g_aheui.y + 2][g_aheui.x], aheui_syllable(AHEUI_OP_MOVE, AHEUI_DIR_UP, AHEUI_MEMORY2));
  aheui_emit_utf8(&g_aheui.block[g_aheui.y + 2][g_aheui.x + (3 * g_aheui.dx)], aheui_syllable(AHEUI_OP_MOVE, (g_aheui.dx == 1) ? AHEUI_DIR_LEFT : AHEUI_DIR_RIGHT, AHEUI_MEMORY2));
  aheui_emit_utf8(&g_aheui.block[g_aheui.y + 2][g_aheui.x + (6 * g_aheui.dx)], aheui_syllable(AHEUI_OP_SELECT, (g_aheui.dx == 1) ? AHEUI_DIR_LEFT : AHEUI_DIR_RIGHT, AHEUI_MEMORY1));
  aheui_emit_utf8(&g_aheui.block[g_aheui.y + 2][g_aheui.x + (15 * g_aheui.dx)], aheui_syllable(AHEUI_OP_BRANCH, (g_aheui.dx == 1) ? AHEUI_DIR_RIGHT : AHEUI_DIR_LEFT, 0));
  aheui_emit_utf8(&g_aheui.block[g_aheui.y + 2][g_aheui.x + (18 * g_aheui.dx)], aheui_syllable(AHEUI_OP_DUP, (g_aheui.dx == 1) ? AHEUI_DIR_RIGHT : AHEUI_DIR_LEFT, 0));
  aheui_emit_utf8(&g_aheui.block[g_aheui.y + 2][g_aheui.x + (21 * g_aheui.dx)], aheui_syllable(AHEUI_OP_SELECT, (g_aheui.dx == 1) ? AHEUI_DIR_RIGHT : AHEUI_DIR_LEFT, AHEUI_MEMORY1));
  aheui_emit_utf8(&g_aheui.block[g_aheui.y + 2][g_aheui.x + (24 * g_aheui.dx)], aheui_syllable(AHEUI_OP_DUP, (g_aheui.dx == 1) ? AHEUI_DIR_RIGHT : AHEUI_DIR_LEFT, 0));
  aheui_emit_utf8(&g_aheui.block[g_aheui.y + 2][g_aheui.x + (27 * g_aheui.dx)], aheui_syllable(AHEUI_OP_MOVE, (g_aheui.dx == 1) ? AHEUI_DIR_RIGHT : AHEUI_DIR_LEFT, 0));
  aheui_emit_utf8(&g_aheui.block[g_aheui.y + 2][g_aheui.x + (30 * g_aheui.dx)], aheui_syllable(AHEUI_OP_SELECT, (g_aheui.dx == 1) ? AHEUI_DIR_RIGHT : AHEUI_DIR_LEFT, 0));
  aheui_emit_utf8(&g_aheui.block[g_aheui.y + 2][g_aheui.x + (33 * g_aheui.dx)], aheui_syllable(AHEUI_OP_SUB, (g_aheui.dx == 1) ? AHEUI_DIR_RIGHT2 : AHEUI_DIR_LEFT2, 0));
  aheui_emit_utf8(&g_aheui.block[g_aheui.y + 2][g_aheui.x + (36 * g_aheui.dx)], aheui_syllable(AHEUI_OP_DUP, AHEUI_DIR_UP, 0));
  aheui_emit_utf8(&g_aheui.block[g_aheui.y + 2][g_aheui.x + (39 * g_aheui.dx)], aheui_syllable(AHEUI_OP_BRANCH, (g_aheui.dx == 1) ? AHEUI_DIR_LEFT : AHEUI_DIR_RIGHT, 0));
  aheui_emit_utf8(&g_aheui.block[g_aheui.y + 2][g_aheui.x + (42 * g_aheui.dx)], aheui_syllable(AHEUI_OP_POP, (g_aheui.dx == 1) ? AHEUI_DIR_RIGHT : AHEUI_DIR_LEFT, 0));
  g_aheui.y += 2;
  g_aheui.x += 45 * g_aheui.dx;
  aheui_emit_op(AHEUI_OP_SELECT, AHEUI_MEMORY1);
  aheui_emit_op(AHEUI_OP_MOVE, AHEUI_MEMORY2);
}

static void aheui_emit_rewind_memory() {
  // 샃썾사
  aheui_make_room(2);
  aheui_emit_utf8(&g_aheui.block[g_aheui.y][g_aheui.x], aheui_syllable(AHEUI_OP_SELECT, (g_aheui.dx == 1) ? AHEUI_DIR_RIGHT : AHEUI_DIR_LEFT, AHEUI_MEMORY2));
  aheui_emit_utf8(&g_aheui.block[g_aheui.y][g_aheui.x + (3 * g_aheui.dx)], aheui_syllable(AHEUI_OP_MOVE, (g_aheui.dx == 1) ? AHEUI_DIR_LEFT : AHEUI_DIR_RIGHT, AHEUI_MEMORY1));
  g_aheui.x += 6 * g_aheui.dx;
  aheui_emit_op(AHEUI_OP_SELECT, 0);
}

static void aheui_emit_inst(Inst* inst) {
  switch (inst->op) {
    case MOV:
      aheui_emit_src(inst);
      aheui_emit_store_reg(inst->dst.reg);
      break;

    case ADD:
      aheui_emit_dst(inst);
      aheui_emit_src(inst);
      aheui_emit_op(AHEUI_OP_ADD, 0);
      aheui_emit_uint_mod();
      aheui_emit_op(AHEUI_OP_MOD, 0);
      aheui_emit_store_reg(inst->dst.reg);
      break;

    case SUB:
      aheui_emit_dst(inst);
      aheui_emit_src(inst);
      aheui_emit_op(AHEUI_OP_SUB, 0);
      aheui_emit_uint_mod();
      aheui_emit_op(AHEUI_OP_ADD, 0);
      aheui_emit_uint_mod();
      aheui_emit_op(AHEUI_OP_MOD, 0);
      aheui_emit_store_reg(inst->dst.reg);
      break;

    case LOAD:
      // (seek)빠싸(rewind)
      aheui_emit_seek_memory(&inst->src);
      aheui_emit_op(AHEUI_OP_DUP, 0);
      aheui_emit_op(AHEUI_OP_MOVE, 0);
      aheui_emit_rewind_memory();

      // (사)load
      aheui_emit_store_reg(inst->dst.reg);

      break;

    case STORE:
      // (seek)마사
      aheui_emit_seek_memory(&inst->src);
      aheui_emit_op(AHEUI_OP_POP, 0);
      aheui_emit_op(AHEUI_OP_SELECT, 0);

      // store
      aheui_emit_dst(inst);
      aheui_emit_op(AHEUI_OP_MOVE, AHEUI_MEMORY1);

      aheui_emit_rewind_memory();
      break;

    case PUTC:
      aheui_emit_src(inst);
      aheui_emit_op(AHEUI_OP_POP, 27);
      break;

    case GETC: {
      /*
      if (g_aheui.dx == 1) {
        // 야우밯부
        // ㅇ반받타
        aheui_emit_utf8(&g_aheui.block[g_aheui.y][g_aheui.x], aheui_syllable(AHEUI_OP_NUL, AHEUI_DIR_RIGHT2, 0));
        aheui_emit_utf8(&g_aheui.block[g_aheui.y][g_aheui.x + 3], aheui_syllable(AHEUI_OP_NUL, AHEUI_DIR_DOWN, 0));
        aheui_emit_utf8(&g_aheui.block[g_aheui.y][g_aheui.x + 6], aheui_syllable(AHEUI_OP_PUSH, AHEUI_DIR_RIGHT, 27));
        aheui_emit_utf8(&g_aheui.block[g_aheui.y][g_aheui.x + 9], aheui_syllable(AHEUI_OP_PUSH, AHEUI_DIR_DOWN, 0));
      } else {
        aheui_emit_utf8(&g_aheui.block[g_aheui.y][g_aheui.x], aheui_syllable(AHEUI_OP_NUL, AHEUI_DIR_LEFT2, 0));
        aheui_emit_utf8(&g_aheui.block[g_aheui.y][g_aheui.x - 3], aheui_syllable(AHEUI_OP_NUL, AHEUI_DIR_DOWN, 0));
        aheui_emit_utf8(&g_aheui.block[g_aheui.y][g_aheui.x - 6], aheui_syllable(AHEUI_OP_PUSH, AHEUI_DIR_LEFT, 27));
        aheui_emit_utf8(&g_aheui.block[g_aheui.y][g_aheui.x - 9], aheui_syllable(AHEUI_OP_PUSH, AHEUI_DIR_DOWN, 0));
      }
      */
      // 밯빠바쟈무차우
      // ㅇㅇㅇㅇ바ㅇ아
      aheui_emit_op(AHEUI_OP_PUSH, 27);
      aheui_emit_op(AHEUI_OP_DUP, 0);
      aheui_emit_op(AHEUI_OP_PUSH, 0);
      aheui_make_room(4);
      aheui_emit_utf8(&g_aheui.block[g_aheui.y][g_aheui.x], aheui_syllable(AHEUI_OP_CMP, (g_aheui.dx == 1) ? AHEUI_DIR_RIGHT2 : AHEUI_DIR_LEFT2, 0));
      aheui_emit_utf8(&g_aheui.block[g_aheui.y][g_aheui.x + 3 * g_aheui.dx], aheui_syllable(AHEUI_OP_POP, AHEUI_DIR_DOWN, 0));
      aheui_emit_utf8(&g_aheui.block[g_aheui.y][g_aheui.x + 6 * g_aheui.dx], aheui_syllable(AHEUI_OP_BRANCH, (g_aheui.dx == 1) ? AHEUI_DIR_RIGHT : AHEUI_DIR_LEFT, 0));
      aheui_emit_utf8(&g_aheui.block[g_aheui.y][g_aheui.x + 9 * g_aheui.dx], aheui_syllable(AHEUI_OP_NUL, AHEUI_DIR_DOWN, 0));
      g_aheui.y++;
      aheui_clear_block_line(g_aheui.y);
      g_aheui.x += g_aheui.dx * 3;
      aheui_emit_op(AHEUI_OP_PUSH, 0);
      aheui_emit_op(AHEUI_OP_NUL, 0);
      aheui_emit_utf8(&g_aheui.block[g_aheui.y][g_aheui.x], aheui_syllable(AHEUI_OP_NUL, (g_aheui.dx == 1) ? AHEUI_DIR_RIGHT : AHEUI_DIR_LEFT, 0));
      // To support implementations which return -1 on EOF.
      // aheui_emit_op(AHEUI_OP_PUSH, aheuinumtojong[2]);
      // aheui_emit_op(AHEUI_OP_PUSH, aheuinumtojong[3]);
      // aheui_emit_op(AHEUI_OP_SUB, 0);
      aheui_emit_store_reg(inst->dst.reg);
      break;
    }

    case EXIT:
      aheui_emit_op(AHEUI_OP_EXIT, 0);
      break;

    case DUMP:
      break;

    case EQ:
    case NE:
    case LT:
    case GT:
    case LE:
    case GE:
      aheui_emit_cmp(inst, 1);
      aheui_emit_store_reg(inst->dst.reg);
      break;

    case JEQ:
    case JNE:
    case JLT:
    case JGT:
    case JLE:
    case JGE:
      aheui_emit_value(&inst->jmp);
      aheui_emit_jmp(inst);
      break;

    case JMP:
      aheui_emit_value(&inst->jmp);
      g_aheui.was_jmp = true;
      break;

    default:
      error("oops");
  }
}

static void aheui_flush_code_block() {
  if (g_aheui.dx == 1) {
    aheui_emit_utf8(&g_aheui.block[g_aheui.y][g_aheui.x], aheui_syllable(AHEUI_OP_NUL, AHEUI_DIR_DOWN, 0));
    g_aheui.y++;
    aheui_clear_block_line(g_aheui.y);
    aheui_emit_utf8(&g_aheui.block[g_aheui.y][g_aheui.x], aheui_syllable(AHEUI_OP_NUL, AHEUI_DIR_LEFT, 0));
  }
  if (g_aheui.was_jmp) {
    aheui_emit_utf8(&g_aheui.block[g_aheui.y][AHEUI_X_CONTROL], aheui_syllable(AHEUI_OP_NUL, AHEUI_DIR_UP, 0));
  } else {
    aheui_emit_utf8(&g_aheui.block[g_aheui.y][AHEUI_X_CONTROL + 3], aheui_syllable(AHEUI_OP_NUL, AHEUI_DIR_DOWN, 0));
  }
  // ㅇ바ㅇ뺘분처마
  // ㅇㅇㅇ투반녀
  aheui_emit_utf8(&g_aheui.block[0][3], aheui_syllable(AHEUI_OP_PUSH, AHEUI_DIR_RIGHT, aheuinumtojong[0]));
  aheui_emit_utf8(&g_aheui.block[0][9], aheui_syllable(AHEUI_OP_DUP, AHEUI_DIR_RIGHT2, 0));
  aheui_emit_utf8(&g_aheui.block[0][12], aheui_syllable(AHEUI_OP_PUSH, AHEUI_DIR_DOWN, aheuinumtojong[2]));
  aheui_emit_utf8(&g_aheui.block[0][15], aheui_syllable(AHEUI_OP_BRANCH, AHEUI_DIR_LEFT, 0));
  aheui_emit_utf8(&g_aheui.block[0][18], aheui_syllable(AHEUI_OP_POP, AHEUI_DIR_RIGHT, 0));
  aheui_emit_utf8(&g_aheui.block[1][9], aheui_syllable(AHEUI_OP_SUB, AHEUI_DIR_DOWN, 0));
  aheui_emit_utf8(&g_aheui.block[1][12], aheui_syllable(AHEUI_OP_PUSH, AHEUI_DIR_RIGHT, aheuinumtojong[2]));
  aheui_emit_utf8(&g_aheui.block[1][15], aheui_syllable(AHEUI_OP_DIV, AHEUI_DIR_LEFT2, 0));
  aheui_block_init();
}

static void aheui_init_state(Data* data) {
  aheui_block_init();

  aheui_emit_op(AHEUI_OP_SELECT, AHEUI_MEMORY2);
  for (uint mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      aheui_emit_num(mp * aheui_memory_step);
      aheui_emit_num(data->v);

      if (g_aheui.y > 90) {
        aheui_flush_code_block();
      }
    }
  }
  aheui_emit_rewind_memory();

  for (uint i = 0; i < 6; i++) {
    aheui_emit_op(AHEUI_OP_SELECT, i + 1);
    aheui_emit_op(AHEUI_OP_PUSH, 0);
  }
  // PC
  aheui_emit_op(AHEUI_OP_SELECT, 0);
  aheui_emit_op(AHEUI_OP_PUSH, 0);

  while (g_aheui.y == 0) {
    aheui_emit_op(AHEUI_OP_NUL, 0);
  }
  aheui_emit_utf8(&g_aheui.block[g_aheui.y][9], aheui_syllable(AHEUI_OP_NUL, AHEUI_DIR_DOWN, 0));
  aheui_emit_utf8(&g_aheui.block[g_aheui.y][AHEUI_X_CONTROL], aheui_syllable(AHEUI_OP_NUL, AHEUI_DIR_LEFT, 0));
}

void target_aheui(Module* module) {
  aheui_init_state(module->data);

  int prev_pc = -1;
  for (Inst* inst = module->text; inst; inst = inst->next) {
    if (prev_pc != inst->pc) {
      if (prev_pc == -1)
        aheui_block_init();
      else
        aheui_flush_code_block();
    }
    prev_pc = inst->pc;
    aheui_emit_inst(inst);
  }

  aheui_flush_code_block();
}
