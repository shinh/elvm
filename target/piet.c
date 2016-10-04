#include <ir/ir.h>
#include <target/util.h>

enum {
  PIET_A = 1,
  PIET_B,
  PIET_C,
  PIET_D,
  PIET_BP,
  PIET_SP,
  PIET_MEM
};

static uint g_piet_label_id;
static uint piet_gen_label() {
  return ++g_piet_label_id;
}

static void piet_label(uint id) {
  emit_line("_track_%u:", id);
}

static void piet_push(int v) {
  emit_line("%d", v);
}

static void piet_pop() {
  emit_line("pop");
}

static void piet_dup() {
  emit_line("dup");
}

static void piet_br(uint id) {
  emit_line("br._track_%u", id);
}

static void piet_bz(uint id) {
  emit_line("bz._track_%u", id);
}

static void piet_roll(uint depth, uint count) {
  emit_line("%d %d roll", depth, count);
}

static void piet_rroll(uint depth, uint count) {
  emit_line("%d -%d roll", depth, count);
}

static void piet_load(uint pos) {
  piet_rroll(pos + 1, 1);
  piet_dup();
  piet_roll(pos + 2, 1);
}

static void piet_store_top(uint pos) {
  piet_rroll(pos + 2, 1);
  piet_pop();
  piet_roll(pos + 1, 1);
}

static void piet_store(uint pos, uint val) {
  piet_push(val);
  piet_store_top(pos);
}

static void piet_init_state(Data* data) {
  //emit_line("16777216");
  emit_line("65536");
  uint loop_id = piet_gen_label();
  uint done_id = piet_gen_label();
  piet_label(loop_id);
  emit_line("1 sub");
  emit_line("dup");
  piet_bz(done_id);
  emit_line("0");
  piet_roll(2, 1);
  piet_br(loop_id);
  piet_label(done_id);
  emit_line("");

  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      piet_store(PIET_MEM + mp, data->v);
    }
  }
}

static void piet_push_value(Value* v, uint stk) {
  if (v->type == REG) {
    piet_load(PIET_A + v->reg + stk);
  } else if (v->type == IMM) {
    piet_push(v->imm);
  } else {
    error("invalid value");
  }
}

static void piet_push_dst(Inst* inst, uint stk) {
  piet_push_value(&inst->dst, stk);
}

static void piet_push_src(Inst* inst, uint stk) {
  piet_push_value(&inst->src, stk);
}

static void piet_uint_mod() {
  //emit_line("16777216 mod");
  emit_line("65536 mod");
}

static void piet_cmp(Inst* inst, bool is_jmp) {
  Op op = normalize_cond(inst->op, is_jmp);
  if (op == JLT) {
    op = JGT;
    piet_push_src(inst, 0);
    piet_push_dst(inst, 1);
  } else if (op == JGE) {
    op = JLE;
    piet_push_src(inst, 0);
    piet_push_dst(inst, 1);
  } else {
    piet_push_dst(inst, 0);
    piet_push_src(inst, 1);
  }
  switch (op) {
  case JEQ:
    emit_line("sub");
    emit_line("not");
    break;
  case JNE:
    emit_line("sub");
    if (!is_jmp) {
      emit_line("not");
      emit_line("not");
    }
    break;
  case JGT:
    emit_line("gt");
    break;
  case JLE:
    emit_line("gt");
    emit_line("not");
    break;
  default:
    error("cmp");
  }
}

static void piet_reg_jmp_table(int min_pc, int max_pc, int last_label) {
  if (min_pc + 1 == max_pc) {
    piet_pop();
    piet_br(min_pc);
    return;
  }

  int mid_pc = (min_pc + max_pc) / 2;
  piet_dup();
  piet_push(mid_pc-1);
  emit_line("gt");
  piet_bz(last_label + mid_pc);
  piet_reg_jmp_table(mid_pc, max_pc, last_label);
  piet_label(last_label + mid_pc);
  piet_reg_jmp_table(min_pc, mid_pc, last_label);
}

static void piet_emit_inst(Inst* inst, int reg_jmp) {
  emit_line("");
  switch (inst->op) {
  case MOV:
    piet_push_src(inst, 0);
    piet_store_top(PIET_A + inst->dst.reg);
    break;

  case ADD:
    piet_push_dst(inst, 0);
    piet_push_src(inst, 1);
    emit_line("add");
    piet_uint_mod();
    piet_store_top(PIET_A + inst->dst.reg);
    break;

  case SUB:
    piet_push_dst(inst, 0);
    piet_push_src(inst, 1);
    emit_line("sub");
    piet_uint_mod();
    piet_store_top(PIET_A + inst->dst.reg);
    break;

  case LOAD:
    piet_push_src(inst, 0);

    piet_push(PIET_MEM + 1);
    emit_line("add");
    emit_line("-1 roll");
    piet_dup();

    piet_push_src(inst, 0);
    piet_push(PIET_MEM + 2);
    emit_line("add");
    emit_line("1 roll");

    piet_store_top(PIET_A + inst->dst.reg);
    break;

  case STORE:
    piet_push_dst(inst, 0);
    piet_push_src(inst, 1);
    piet_dup();

    piet_push(PIET_MEM + 3);
    emit_line("add");
    emit_line("-1 roll");
    piet_pop();

    piet_push(PIET_MEM + 1);
    emit_line("add");
    emit_line("1 roll");
    break;

  case PUTC:
    piet_push_src(inst, 0);
    emit_line("out");
    break;

  case GETC: {
    piet_push(256);
    emit_line("in");
    piet_dup();
    piet_push(256);
    emit_line("sub");

    uint zero_id = piet_gen_label();
    uint done_id = piet_gen_label();

    piet_bz(zero_id);
    piet_roll(2, 1);
    piet_pop();
    piet_br(done_id);

    piet_label(zero_id);
    piet_pop();
    piet_push(0);

    piet_label(done_id);
    piet_store_top(PIET_A + inst->dst.reg);

    break;
  }

  case EXIT:
    emit_line("halt");
    break;

  case DUMP:
    break;

  case EQ:
  case NE:
  case LT:
  case GT:
  case LE:
  case GE:
    piet_cmp(inst, false);
    piet_store_top(PIET_A + inst->dst.reg);
    break;

  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
    piet_cmp(inst, true);
    if (inst->jmp.type == REG) {
      error("jcc reg");
    } else {
      piet_bz(inst->jmp.imm);
    }
    break;

  case JMP:
    if (inst->jmp.type == REG) {
      piet_push_value(&inst->jmp, 0);
      piet_br(reg_jmp);
    } else {
      piet_br(inst->jmp.imm);
    }
    break;

  default:
    error("oops");
  }
}

void target_piet(Module* module) {
  for (Inst* inst = module->text; inst; inst = inst->next) {
    g_piet_label_id = inst->pc;
  }

  int reg_jmp = piet_gen_label();
  piet_init_state(module->data);

  int prev_pc = -1;
  for (Inst* inst = module->text; inst; inst = inst->next) {
    if (prev_pc != inst->pc) {
      piet_label(inst->pc);
    }
    prev_pc = inst->pc;

    piet_emit_inst(inst, reg_jmp);
  }

  piet_label(reg_jmp);
  piet_reg_jmp_table(0, reg_jmp, piet_gen_label());
}
