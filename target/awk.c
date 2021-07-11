#include <ir/ir.h>
#include <target/util.h>

static void init_state_awk(Data* data) {
  emit_line("@load \"ordchr\"");
  emit_line("BEGIN {");
  inc_indent();
  for (int i = 0; i < 7; i++) {
    emit_line("%s = 0", reg_names[i]);
  }
  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      emit_line("mem[%d] = %d", mp, data->v);
    }
  }
  emit_line("RS = \"[\\x01-\\xff]\"");
  dec_indent();
  emit_line("}");
  emit_line("");
  emit_line("function getchar() {");
  inc_indent();
  emit_line("if (getline <= 0) { return 0 }");
  emit_line("return ord(RT)");
  dec_indent();
  emit_line("}");
}

static void awk_emit_func_prologue(int func_id) {
  emit_line("");
  emit_line("function func%d() {", func_id);
  inc_indent();
  emit_line("while (%d <= pc && pc < %d) {",
            func_id * CHUNKED_FUNC_SIZE, (func_id + 1) * CHUNKED_FUNC_SIZE);
  inc_indent();
  emit_line("switch (pc) {");
  emit_line("case -1:");
  inc_indent();
}

static void awk_emit_func_epilogue(void) {
  dec_indent();
  emit_line("}");
  emit_line("pc += 1");
  dec_indent();
  emit_line("}");
  dec_indent();
  emit_line("}");
}

static void awk_emit_pc_change(int pc) {
  emit_line("break");
  dec_indent();
  emit_line("");
  emit_line("case %d:", pc);
  inc_indent();
}

static void awk_emit_inst(Inst* inst) {
  switch (inst->op) {
  case MOV:
    emit_line("%s = %s", reg_names[inst->dst.reg], src_str(inst));
    break;

  case ADD:
    emit_line("%s = and(%s + %s, " UINT_MAX_STR ")",
              reg_names[inst->dst.reg],
              reg_names[inst->dst.reg], src_str(inst));
    break;

  case SUB:
    emit_line("if (%s < %s) {", reg_names[inst->dst.reg], src_str(inst));
    inc_indent();
    emit_line("%s = and(" UINT_MAX_STR " + 1 + %s - %s, " UINT_MAX_STR ")",
              reg_names[inst->dst.reg],
              reg_names[inst->dst.reg], src_str(inst));
    dec_indent();
    emit_line("} else {");
    inc_indent();
    emit_line("%s = and(%s - %s, " UINT_MAX_STR ")",
              reg_names[inst->dst.reg],
              reg_names[inst->dst.reg], src_str(inst));
    dec_indent();
    emit_line("}");
    break;

  case LOAD:
    emit_line("%s = mem[%s]", reg_names[inst->dst.reg], src_str(inst));
    break;

  case STORE:
    emit_line("mem[%s] = %s", src_str(inst), reg_names[inst->dst.reg]);
    break;

  case PUTC:
    emit_line("printf \"%%s\", chr(%s)", src_str(inst));
    break;

  case GETC:
    emit_line("%s = getchar()", reg_names[inst->dst.reg]);
    break;

  case EXIT:
    emit_line("exit(0)");
    break;

  case DUMP:
    break;

  case EQ:
  case NE:
  case LT:
  case GT:
  case LE:
  case GE:
    emit_line("%s = %s ? 1 : 0",
              reg_names[inst->dst.reg], cmp_str(inst, "1"));
    break;

  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
  case JMP:
    emit_line("if (%s) { pc = %s - 1 }",
              cmp_str(inst, "1"), value_str(&inst->jmp));
    break;

  default:
    error("oops");
  }
}

void target_awk(Module* module) {
  init_state_awk(module->data);
  emit_line("");

  int num_funcs = emit_chunked_main_loop(module->text,
                                         awk_emit_func_prologue,
                                         awk_emit_func_epilogue,
                                         awk_emit_pc_change,
                                         awk_emit_inst);

  emit_line("");
  emit_line("BEGIN {");
  inc_indent();
  emit_line("while (1) {");
  inc_indent();
  emit_line("switch (int(pc / %d)) {", CHUNKED_FUNC_SIZE);
  for (int i = 0; i < num_funcs; i++) {
    emit_line("case %d: func%d(); break", i, i);
  }
  emit_line("}");
  dec_indent();
  emit_line("}");
  dec_indent();
  emit_line("}");
}
