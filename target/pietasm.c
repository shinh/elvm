#include <ir/ir.h>
#include <target/util.h>

enum {
  PIETASM_A = 1,
  PIETASM_B,
  PIETASM_C,
  PIETASM_D,
  PIETASM_BP,
  PIETASM_SP,
  PIETASM_MEM
};

static uint g_pietasm_label_id;
static uint pietasm_gen_label() {
  return ++g_pietasm_label_id;
}

static void pietasm_label(uint id) {
  emit_line("_track_%u:", id);
}

static void pietasm_push(int v) {
  emit_line("%d", v);
}

static void pietasm_pop() {
  emit_line("pop");
}

static void pietasm_dup() {
  emit_line("dup");
}

static void pietasm_br(uint id) {
  emit_line("br._track_%u", id);
}

static void pietasm_bz(uint id) {
  emit_line("bz._track_%u", id);
}

static void pietasm_roll(uint depth, uint count) {
  emit_line("%d %d roll", depth, count);
}

static void pietasm_rroll(uint depth, uint count) {
  emit_line("%d -%d roll", depth, count);
}

static void pietasm_load(uint pos) {
  pietasm_rroll(pos + 1, 1);
  pietasm_dup();
  pietasm_roll(pos + 2, 1);
}

static void pietasm_store_top(uint pos) {
  pietasm_rroll(pos + 2, 1);
  pietasm_pop();
  pietasm_roll(pos + 1, 1);
}

static void pietasm_store(uint pos, uint val) {
  pietasm_push(val);
  pietasm_store_top(pos);
}

static void pietasm_init_state(Data* data) {
  //emit_line("16777223");
  emit_line("65543");  // 65536 + 7
  uint loop_id = pietasm_gen_label();
  uint done_id = pietasm_gen_label();
  pietasm_label(loop_id);
  emit_line("1 sub");
  emit_line("dup");
  pietasm_bz(done_id);
  emit_line("0");
  pietasm_roll(2, 1);
  pietasm_br(loop_id);
  pietasm_label(done_id);
  emit_line("");

  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      pietasm_store(PIETASM_MEM + mp, data->v % 65536);
    }
  }
}

static void pietasm_push_value(Value* v, uint stk) {
  if (v->type == REG) {
    pietasm_load(PIETASM_A + v->reg + stk);
  } else if (v->type == IMM) {
    pietasm_push(v->imm % 65536);
  } else {
    error("invalid value");
  }
}

static void pietasm_push_dst(Inst* inst, uint stk) {
  pietasm_push_value(&inst->dst, stk);
}

static void pietasm_push_src(Inst* inst, uint stk) {
  pietasm_push_value(&inst->src, stk);
}

static void pietasm_uint_mod() {
  //emit_line("16777216 mod");
  emit_line("65536 mod");
}

static void pietasm_cmp(Inst* inst, bool is_jmp) {
  Op op = normalize_cond(inst->op, is_jmp);
  if (op == JLT) {
    op = JGT;
    pietasm_push_src(inst, 0);
    pietasm_push_dst(inst, 1);
  } else if (op == JGE) {
    op = JLE;
    pietasm_push_src(inst, 0);
    pietasm_push_dst(inst, 1);
  } else {
    pietasm_push_dst(inst, 0);
    pietasm_push_src(inst, 1);
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

static void pietasm_reg_jmp_table(int min_pc, int max_pc, int last_label) {
  if (min_pc + 1 == max_pc) {
    pietasm_pop();
    pietasm_br(min_pc);
    return;
  }

  int mid_pc = (min_pc + max_pc) / 2;
  pietasm_dup();
  pietasm_push(mid_pc-1);
  emit_line("gt");
  pietasm_bz(last_label + mid_pc);
  pietasm_reg_jmp_table(mid_pc, max_pc, last_label);
  pietasm_label(last_label + mid_pc);
  pietasm_reg_jmp_table(min_pc, mid_pc, last_label);
}

static void pietasm_emit_inst(Inst* inst, int reg_jmp) {
  emit_line("");
  switch (inst->op) {
  case MOV:
    pietasm_push_src(inst, 0);
    pietasm_store_top(PIETASM_A + inst->dst.reg);
    break;

  case ADD:
    pietasm_push_dst(inst, 0);
    pietasm_push_src(inst, 1);
    emit_line("add");
    pietasm_uint_mod();
    pietasm_store_top(PIETASM_A + inst->dst.reg);
    break;

  case SUB:
    pietasm_push_dst(inst, 0);
    pietasm_push_src(inst, 1);
    emit_line("sub");
    pietasm_uint_mod();
    pietasm_store_top(PIETASM_A + inst->dst.reg);
    break;

  case LOAD:
    pietasm_push_src(inst, 0);
    pietasm_dup();
    // Put the address to the bottom of the stack.
    emit_line("65544 1 roll");

    pietasm_push(PIETASM_MEM + 1);
    emit_line("add");
    emit_line("-1 roll");
    pietasm_dup();

    // Get the address from the bottom of the stack.
    emit_line("65544 -1 roll");
    pietasm_push(PIETASM_MEM + 2);
    emit_line("add");
    emit_line("1 roll");

    pietasm_store_top(PIETASM_A + inst->dst.reg);
    break;

  case STORE:
    pietasm_push_dst(inst, 0);
    pietasm_push_src(inst, 1);
    pietasm_dup();

    pietasm_push(PIETASM_MEM + 3);
    emit_line("add");
    emit_line("-1 roll");
    pietasm_pop();

    pietasm_push(PIETASM_MEM + 1);
    emit_line("add");
    emit_line("1 roll");
    break;

  case PUTC:
    pietasm_push_src(inst, 0);
    emit_line("out");
    break;

  case GETC: {
    pietasm_push(256);
    emit_line("in");
    pietasm_dup();
    pietasm_push(256);
    emit_line("sub");

    uint zero_id = pietasm_gen_label();
    uint done_id = pietasm_gen_label();

    pietasm_bz(zero_id);
    pietasm_roll(2, 1);
    pietasm_pop();
    pietasm_br(done_id);

    pietasm_label(zero_id);
    pietasm_pop();
    pietasm_push(0);

    pietasm_label(done_id);
    pietasm_store_top(PIETASM_A + inst->dst.reg);

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
    pietasm_cmp(inst, false);
    pietasm_store_top(PIETASM_A + inst->dst.reg);
    break;

  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
    pietasm_cmp(inst, true);
    if (inst->jmp.type == REG) {
      error("jcc reg");
    } else {
      pietasm_bz(inst->jmp.imm);
    }
    break;

  case JMP:
    if (inst->jmp.type == REG) {
      pietasm_push_value(&inst->jmp, 0);
      pietasm_br(reg_jmp);
    } else {
      pietasm_br(inst->jmp.imm);
    }
    break;

  default:
    error("oops");
  }
}

void target_pietasm(Module* module) {
  for (Inst* inst = module->text; inst; inst = inst->next) {
    g_pietasm_label_id = inst->pc;
  }

  int reg_jmp = pietasm_gen_label();
  pietasm_init_state(module->data);

  int prev_pc = -1;
  for (Inst* inst = module->text; inst; inst = inst->next) {
    if (prev_pc != inst->pc) {
      pietasm_label(inst->pc);
    }
    prev_pc = inst->pc;

    pietasm_emit_inst(inst, reg_jmp);
  }

  pietasm_label(reg_jmp);
  pietasm_reg_jmp_table(0, reg_jmp, pietasm_gen_label());
}
