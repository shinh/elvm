#include <ir/ir.h>
#include <target/util.h>

#define FORTH_MEM_SIZE_STR "16777216"
#define FORTH_ADDR_MASK UINT_MAX

static const char* FORTH_REG_NAMES[] = {
  "reg-a", "reg-b", "reg-c", "reg-d", "reg-bp", "reg-sp", "reg-pc"
};

static void init_state_forth(Data* data) {
  reg_names = FORTH_REG_NAMES;
  for (int i = 0; i < 7; i++) {
    emit_line("variable %s", reg_names[i]);
    emit_line("0 %s !", reg_names[i]);
  }
  emit_line("variable mem");
  emit_line(FORTH_MEM_SIZE_STR " cells allocate throw mem !");
  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      emit_line("%d mem @ %d cells + !", data->v, mp);
    }
  }
  emit_line("");
  emit_line("variable tmpchar");
  emit_line(": getc");
  inc_indent();
  emit_line("tmpchar 1 stdin read-file");
  emit_line("0= swap 1 = and if tmpchar @ else 0 then");
  dec_indent();
  emit_line(";");
}

static void forth_emit_func_prologue(int func_id) {
  emit_line("");
  emit_line(": func%d", func_id);
  inc_indent();
  emit_line("begin");
  inc_indent();
  emit_line("reg-pc @ dup %d >= swap %d < and",
            func_id * CHUNKED_FUNC_SIZE, (func_id + 1) * CHUNKED_FUNC_SIZE);
  dec_indent();
  emit_line("while");
  inc_indent();
  emit_line("reg-pc @");
  emit_line("dup -1 = if");
  inc_indent();
}

static void forth_emit_func_epilogue(void) {
  dec_indent();
  emit_line("then");
  emit_line("drop");
  emit_line("1 reg-pc +!");
  dec_indent();
  emit_line("repeat");
  dec_indent();
  emit_line(";");
}

static void forth_emit_pc_change(int pc) {
  dec_indent();
  emit_line("then");
  emit_line("dup %d = if", pc);
  inc_indent();
}

static const char* forth_value_str(Value* v) {
  if (v->type == REG) {
    return format("%s @", reg_names[v->reg]);
  } else if (v->type == IMM) {
    return format("%d", v->imm);
  } else {
    error("invalid value");
  }
}

static const char* forth_src_str(Inst* inst) {
  return forth_value_str(&inst->src);
}

static void emit_cmp(Inst* inst, const char* cmp_op) {
  emit_line("%s @ %s %s if 1 else 0 then %s !",
            reg_names[inst->dst.reg], forth_src_str(inst),
            cmp_op, reg_names[inst->dst.reg]);
}

static void emit_jump(Inst* inst) {
  emit_line("%s 1 - reg-pc !", forth_value_str(&inst->jmp));
}

static void emit_cond_jump(Inst* inst, const char* cmp_op) {
  emit_line("%s @ %s %s if",
            reg_names[inst->dst.reg], forth_src_str(inst), cmp_op);
  inc_indent();
  emit_jump(inst);
  dec_indent();
  emit_line("then");
}

static void forth_emit_inst(Inst* inst) {
  switch (inst->op) {
  case MOV:
    emit_line("%s %s !", forth_src_str(inst), reg_names[inst->dst.reg]);
    break;

  case ADD:
    emit_line("%s @ %s + %d and",
              reg_names[inst->dst.reg], forth_src_str(inst), FORTH_ADDR_MASK);
    emit_line("%s !", reg_names[inst->dst.reg]);
    break;

  case SUB:
    emit_line("%s @ %s - %d and",
              reg_names[inst->dst.reg], forth_src_str(inst), FORTH_ADDR_MASK);
    emit_line("%s !", reg_names[inst->dst.reg]);
    break;

  case LOAD:
    emit_line("mem @ %s cells + @ %s !",
              forth_src_str(inst), reg_names[inst->dst.reg]);
    break;

  case STORE:
    emit_line("%s @ mem  @ %s cells + !",
              reg_names[inst->dst.reg], forth_src_str(inst));
    break;

  case PUTC:
    emit_line("%s emit", forth_src_str(inst));
    break;

  case GETC:
    emit_line("getc %s !", reg_names[inst->dst.reg]);
    break;

  case EXIT:
    emit_line("bye");
    break;

  case DUMP:
    break;

  case EQ:
    emit_cmp(inst, "=");
    break;
  case NE:
    emit_cmp(inst, "<>");
    break;
  case LT:
    emit_cmp(inst, "<");
    break;
  case GT:
    emit_cmp(inst, ">");
    break;
  case LE:
    emit_cmp(inst, "<=");
    break;
  case GE:
    emit_cmp(inst, ">=");
    break;

  case JEQ:
    emit_cond_jump(inst, "=");
    break;
  case JNE:
    emit_cond_jump(inst, "<>");
    break;
  case JLT:
    emit_cond_jump(inst, "<");
    break;
  case JGT:
    emit_cond_jump(inst, ">");
    break;
  case JLE:
    emit_cond_jump(inst, "<=");
    break;
  case JGE:
    emit_cond_jump(inst, ">=");
    break;
  case JMP:
    emit_jump(inst);
    break;

  default:
    error("oops");
  }
}

void target_forth(Module* module) {
  init_state_forth(module->data);
  emit_line("");

  int num_funcs = emit_chunked_main_loop(module->text,
                                         forth_emit_func_prologue,
                                         forth_emit_func_epilogue,
                                         forth_emit_pc_change,
                                         forth_emit_inst);

  emit_line("");
  emit_line(": main");
  inc_indent();
  emit_line("begin");
  inc_indent();
  emit_line("reg-pc @ %d /", CHUNKED_FUNC_SIZE);
  for (int i = 0; i < num_funcs; i++) {
    emit_line("dup %d = if func%d else", i, i);
  }
  for (int i = 0; i < num_funcs; i++) {
    emit_line("then");
  }
  emit_line("drop");
  dec_indent();
  emit_line("0 until");
  dec_indent();
  emit_line(";");
  emit_line("");
  emit_line("main");
}
