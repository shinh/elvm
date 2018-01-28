#include <stdarg.h>

#include <ir/ir.h>
#include <target/util.h>

static const char* I_REG_NAMES[] = {
  ":1", ":2", ":3", ":4", ":5", ":6", ":7"
};

// :8  = tmp dst
// :9  = tmp src
// :10 = interleave(#0, #65535)
// :11 = reg jmp
// :12 = prev putc
// :13 = prev getc
// ;1  = memory
// ;2  = putc table
// ;3  = getc table (for CLC-INTERCAL only)
// ;4  = temporary buffer for getc/putc

#define USE_C_INTERCAL

#if defined(USE_C_INTERCAL)

#define I_INT "$"

static const int I_PUTC_TBL[] = {
  256,128,192,64,224,96,160,32,240,112,176,48,208,80,144,16,
  248,120,184,56,216,88,152,24,232,104,168,40,200,72,136,8,
  252,124,188,60,220,92,156,28,236,108,172,44,204,76,140,12,
  244,116,180,52,212,84,148,20,228,100,164,36,196,68,132,4,
  254,126,190,62,222,94,158,30,238,110,174,46,206,78,142,14,
  246,118,182,54,214,86,150,22,230,102,166,38,198,70,134,6,
  250,122,186,58,218,90,154,26,234,106,170,42,202,74,138,10,
  242,114,178,50,210,82,146,18,226,98,162,34,194,66,130,2,
  255,127,191,63,223,95,159,31,239,111,175,47,207,79,143,15,
  247,119,183,55,215,87,151,23,231,103,167,39,199,71,135,7,
  251,123,187,59,219,91,155,27,235,107,171,43,203,75,139,11,
  243,115,179,51,211,83,147,19,227,99,163,35,195,67,131,3,
  253,125,189,61,221,93,157,29,237,109,173,45,205,77,141,13,
  245,117,181,53,213,85,149,21,229,101,165,37,197,69,133,5,
  249,121,185,57,217,89,153,25,233,105,169,41,201,73,137,9,
  241,113,177,49,209,81,145,17,225,97,161,33,193,65,129,1
};

#else

#define I_INT "C\x08/"

static const int I_PUTC_TBL[] = {
  85,84,81,80,87,86,83,82,93,92,89,88,95,94,91,90,
  69,68,65,64,71,70,67,66,77,76,73,72,79,78,75,74,
  117,116,113,112,119,118,115,114,125,124,121,120,127,126,123,122,
  101,100,97,96,103,102,99,98,109,108,105,104,111,110,107,106,
  21,20,17,16,23,22,19,18,29,28,25,24,31,30,27,26,
  5,4,1,256,7,6,3,2,13,12,9,8,15,14,11,10,
  53,52,49,48,55,54,51,50,61,60,57,56,63,62,59,58,
  37,36,33,32,39,38,35,34,45,44,41,40,47,46,43,42,
  213,212,209,208,215,214,211,210,221,220,217,216,223,222,219,218,
  197,196,193,192,199,198,195,194,205,204,201,200,207,206,203,202,
  245,244,241,240,247,246,243,242,253,252,249,248,255,254,251,250,
  229,228,225,224,231,230,227,226,237,236,233,232,239,238,235,234,
  149,148,145,144,151,150,147,146,157,156,153,152,159,158,155,154,
  133,132,129,128,135,134,131,130,141,140,137,136,143,142,139,138,
  181,180,177,176,183,182,179,178,189,188,185,184,191,190,187,186,
  165,164,161,160,167,166,163,162,173,172,169,168,175,174,171,170
};

#endif

static uint i_interleave(uint a, uint b) {
  uint r = 0;
  uint m = 1;
  while (a > 0 || b > 0) {
    r += (a % 2 * 2 + b % 2) * m;
    m *= 4;
    a /= 2;
    b /= 2;
  }
  return r;
}

static uint i_emit_please_cnt;
static void i_emit_line(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  char* r = vformat(fmt, ap);
  va_end(ap);

  if (++i_emit_please_cnt == 3) {
    printf("PLEASE ");
    i_emit_please_cnt = 0;
  }
  printf("DO %s\n", r);
}

static char* i_imm(uint v) {
  return format("#%d", v % 65536);
#if 0
  if (v < 65536) {
    return format("#%d", v);
  }
  uint a = 0, b = 0;
  while (v) {
    uint b0 = v % 2;
    v /= 2;
    uint a0 = v % 2;
    v /= 2;
    b = b * 2 + b0;
    a = a * 2 + a0;
  }
  return format("#%d"I_INT"#%d", a, b);
#endif
}

static void i_init_state(Data* data) {
  for (int i = 0; i < 7; i++) {
    if (i == SP) {
      // INTERCAL backend cannot access mem[65535].
      // TODO: Adjust memory layout to allow this.
      i_emit_line("%s <- #65534", I_REG_NAMES[i]);
    } else {
      i_emit_line("%s <- #0", I_REG_NAMES[i]);
    }
  }
  i_emit_line(":10 <- #0"I_INT"#65535");
  i_emit_line(":12 <- #0");
  i_emit_line(":13 <- #0");

  //i_emit_line(";1 <- #0"I_INT"#4096");
  i_emit_line(";1 <- #65535");
  for (int mp = 1; data; data = data->next, mp++) {
    i_emit_line(";1 SUB #%d <- %s", mp, i_imm(data->v));
  }

  i_emit_line(";2 <- #%d", i_interleave(1, 255) + 1);
  i_emit_line(";3 <- #%d", i_interleave(1, 255) + 1);
  for (int i = 1; i < 256; i++) {
    i_emit_line(";2 SUB #%d <- #%d", i_interleave(1, i), I_PUTC_TBL[i]);
    i_emit_line(";3 SUB #%d <- #%d", i_interleave(1, I_PUTC_TBL[i] % 256), i);
  }
}

static const char* i_value_str(Value* v) {
  if (v->type == REG) {
    return I_REG_NAMES[v->reg];
  } else if (v->type == IMM) {
    return i_imm(v->imm);
  } else {
    error("invalid value");
  }
}

static const char* i_src_str(Inst* inst) {
  return i_value_str(&inst->src);
}

static void i_emit_bitop(const char* op, int dst,
                         const char* a, const char* b) {
  i_emit_line(":%d <- %s " I_INT " %s", dst, a, b);
  i_emit_line(":%d <- ':%s%d' ~ :10", dst, op, dst);
}

static void i_emit_and(int dst, const char* a, const char* b) {
  i_emit_bitop("&", dst, a, b);
}

static void i_emit_xor(int dst, const char* a, const char* b) {
  i_emit_bitop("V\x08-", dst, a, b);
}

static void i_emit_add(int add_fn) {
  i_emit_line("(%d) NEXT", add_fn);
}

static void i_emit_sub(int add_fn) {
  i_emit_xor(9, ":9", "#65535");
  i_emit_line(":9 <- :9 ~ #65535");
  i_emit_add(add_fn);
  i_emit_line(":9 <- #1");
  i_emit_add(add_fn);
}

// Converts 0, 1 in :8 to 1, 2.
static void i_emit_intercal_boolize() {
  i_emit_line(":8 <- :8" I_INT "#1");
  i_emit_line(":8 <- :V\x08-8");
  i_emit_line(":8 <- :8 ~ #3");
}

static void i_emit_cmp(Inst* inst, int add_fn) {
  int op = normalize_cond(inst->op, false);
  if (op == JGT || op == JLE) {
    op = op == JGT ? JLT : JGE;
    i_emit_line(":9 <- %s", I_REG_NAMES[inst->dst.reg]);
    i_emit_line(":8 <- %s", i_src_str(inst));
  } else {
    i_emit_line(":8 <- %s", I_REG_NAMES[inst->dst.reg]);
    i_emit_line(":9 <- %s", i_src_str(inst));
  }
  i_emit_sub(add_fn);

  switch (op) {
    case JEQ:
      i_emit_line(":8 <- #65535 ~ :8");
      i_emit_line(":8 <- :8 ~ #1");
      i_emit_xor(8, ":8", "#1");
      break;

    case JNE:
      i_emit_line(":8 <- #65535 ~ :8");
      i_emit_line(":8 <- :8 ~ #1");
      break;

    case JGE:
      i_emit_xor(8, ":8", "#32768");
      FALLTHROUGH;

    case JLT:
      i_emit_and(8, ":8", "#32768");
      i_emit_line(":8 <- #32768 ~ :8");
      break;

    default:
      error("oops %d", op);
  }
}

static void i_emit_jmp(Inst* inst, int reg_jmp) {
  if (inst->jmp.type == REG) {
    i_emit_line(":11 <- %s", I_REG_NAMES[inst->jmp.reg]);
    i_emit_line("(%d) NEXT", reg_jmp);
  } else {
    i_emit_line("(%d) NEXT", inst->jmp.imm);
  }
}

static void i_emit_inst(Inst* inst, int add_fn, int reg_jmp, int* label) {
  switch (inst->op) {
  case MOV:
    i_emit_line("%s <- %s", I_REG_NAMES[inst->dst.reg], i_src_str(inst));
    break;

  case ADD:
    i_emit_line(":8 <- %s", I_REG_NAMES[inst->dst.reg]);
    i_emit_line(":9 <- %s", i_src_str(inst));
    i_emit_add(add_fn);
    i_emit_line("%s <- :8", I_REG_NAMES[inst->dst.reg]);
    break;

  case SUB:
    i_emit_line(":8 <- %s", I_REG_NAMES[inst->dst.reg]);
    i_emit_line(":9 <- %s", i_src_str(inst));
    i_emit_sub(add_fn);
    i_emit_line("%s <- :8", I_REG_NAMES[inst->dst.reg]);
    break;

  case LOAD:
    i_emit_line(":8 <- %s", i_src_str(inst));
    i_emit_line(":9 <- #1");
    i_emit_add(add_fn);
    i_emit_line("%s <- ;1 SUB :8", I_REG_NAMES[inst->dst.reg]);
    break;

  case STORE:
    i_emit_line(":8 <- %s", i_src_str(inst));
    i_emit_line(":9 <- #1");
    i_emit_add(add_fn);
    i_emit_line(";1 SUB :8 <- %s", I_REG_NAMES[inst->dst.reg]);
    break;

  case PUTC:
#if defined(USE_C_INTERCAL)
    i_emit_line(":9 <- %s~#255", i_src_str(inst));
    i_emit_line(":9 <- #1"I_INT":9");
    i_emit_line(":8 <- ;2 SUB :9");
    i_emit_line(":9 <- :12");
    i_emit_line(":12 <- :8");
    i_emit_sub(add_fn);
    i_emit_line(";4 <- #1");
    i_emit_line(";4 SUB #1 <- :8");
    i_emit_line("READ OUT ;4");
#else
    i_emit_line(":9 <- %s~#255", i_src_str(inst));
    i_emit_line(":9 <- #1"I_INT":9");
    i_emit_line(";4 <- #1");
    i_emit_line(";4 SUB #1 <- ;2 SUB :9");
    i_emit_line("READ OUT ;4");
#endif
    break;

  case GETC:
    i_emit_line(";4 <- #1");
    i_emit_line(";4 SUB #1 <- #0");
    i_emit_line("WRITE IN ;4");
#if defined(USE_C_INTERCAL)
    i_emit_line(":8 <- ;4 SUB #1");

    i_emit_line(":7 <- :8 ~ #256");
    for (int i = 0; i < 16; i++) {
      i_emit_line(":7 <- :7"I_INT":7");
      i_emit_line(":7 <- :7 ~ #65535");
    }
    i_emit_xor(7, ":7", "#255");
    i_emit_line(":7 <- :7 ~ #255");

    i_emit_line(":9 <- :13");
    i_emit_add(add_fn);
    i_emit_line(":13 <- :8");

    i_emit_and(8, ":8", ":7");

    i_emit_line("%s <- :8", I_REG_NAMES[inst->dst.reg]);
#else
    i_emit_line(":9 <- ;4 SUB #1");
    i_emit_line(":9 <- :9~#255");
    i_emit_line(":9 <- #1"I_INT":9");
    i_emit_line("%s <- ;3 SUB :9", I_REG_NAMES[inst->dst.reg]);
#endif
    break;

  case EXIT:
    i_emit_line("GIVE UP");
    break;

  case DUMP:
    break;

  case EQ:
  case NE:
  case LT:
  case GT:
  case LE:
  case GE:
    i_emit_cmp(inst, add_fn);
    i_emit_line("%s <- :8", I_REG_NAMES[inst->dst.reg]);
    break;

  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE: {
    if (inst->jmp.type == REG) {
      error("jcc reg");
    }

    i_emit_cmp(inst, add_fn);
    i_emit_intercal_boolize();

    int l1 = ++*label;
    int l2 = ++*label;
    i_emit_line("(%d) NEXT", l1);
    i_emit_line("(%d) NEXT", inst->jmp.imm);

    emit_line("(%d) DO RESUME :8", l2);
    emit_line("(%d) DO (%d) NEXT", l1, l2);
    i_emit_line("FORGET #1");
    break;
  }

  case JMP: {
    i_emit_jmp(inst, reg_jmp);
    break;
  }

  default:
    error("oops");
  }
}

static void i_emit_reg_jmp_table(uint pc, uint bit, uint max_pc, int* label) {
  if (bit > max_pc) {
    if (pc >= max_pc || pc == 0) {
      i_emit_line("ERR %d", pc);
    } else {
      i_emit_line("(%d) NEXT", pc);
    }
    return;
  }

  i_emit_line(":8 <- :11 ~ #%d", bit);
  i_emit_intercal_boolize();

  int l1 = ++*label;
  int l2 = ++*label;
  i_emit_line("(%d) NEXT", l1);

  i_emit_reg_jmp_table(pc + bit, bit * 2, max_pc, label);

  emit_line("(%d) DO RESUME :8", l2);
  emit_line("(%d) DO (%d) NEXT", l1, l2);
  i_emit_line("FORGET #1");

  i_emit_reg_jmp_table(pc, bit * 2, max_pc, label);
}

void target_i(Module* module) {
  i_init_state(module->data);

  int label = 0;
  for (Inst* inst = module->text; inst; inst = inst->next) {
    label = inst->pc;
  }
  int reg_jmp = ++label;
  int add_fn = ++label;

  int prev_pc = 0;
  for (Inst* inst = module->text; inst; inst = inst->next) {
    if (prev_pc != inst->pc) {
      emit_line("(%d) DO FORGET #1", inst->pc);
    }
    prev_pc = inst->pc;
    emit_line("");
    i_emit_inst(inst, add_fn, reg_jmp, &label);
  }

  emit_line("");
  emit_line("(%d) DO NOTe add function", add_fn);
  for (int i = 0; i < 16; i++) {
    i_emit_line(":9 <- :8"I_INT":9");
    i_emit_line(":8 <- :V\x08-9");
    i_emit_line(":8 <- :8 ~ :10");
    i_emit_line(":9 <- :&9");
    i_emit_line(":9 <- :9 ~ :10");
    i_emit_line(":9 <- ':9"I_INT"#0'~'#32767"I_INT"#1'");
  }
  i_emit_line("RESUME #1");

  emit_line("");
  i_emit_line("NOTe reg jmp");
  emit_line("(%d) DO FORGET #1", reg_jmp);
  i_emit_reg_jmp_table(0, 1, reg_jmp, &label);
}
