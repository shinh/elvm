#include <ir/ir.h>
#include <target/util.h>

static void c_init_state(void) {
  emit_line("#include <stdio.h>");
  emit_line("#include <stdlib.h>");

  for (int i = 0; i < 7; i++) {
    emit_line("unsigned int %s;", reg_names[i]);
  }
  emit_line("unsigned int mem[1<<24];");
}

static void c_emit_func_prologue(int func_id) {
  emit_line("");
  emit_line("void func%d() {", func_id);
  inc_indent();
  emit_line("while (%d <= pc && pc < %d) {",
            func_id * CHUNKED_FUNC_SIZE, (func_id + 1) * CHUNKED_FUNC_SIZE);
  inc_indent();
  emit_line("switch (pc) {");
  emit_line("case -1:  /* dummy */");
  inc_indent();
}

static void c_emit_func_epilogue(void) {
  dec_indent();
  emit_line("}");
  emit_line("pc++;");
  dec_indent();
  emit_line("}");
  dec_indent();
  emit_line("}");
}

static void c_emit_pc_change(int pc) {
  emit_line("break;");
  emit_line("");
  dec_indent();
  emit_line("case %d:", pc);
  inc_indent();
}

static void c_emit_inst(Inst* inst) {
  switch (inst->op) {
  case MOV:
    emit_line("%s = %s;", reg_names[inst->dst.reg], src_str(inst));
    break;

  case ADD:
    emit_line("%s = (%s + %s) & " UINT_MAX_STR ";",
              reg_names[inst->dst.reg],
              reg_names[inst->dst.reg], src_str(inst));
    break;

  case SUB:
    emit_line("%s = (%s - %s) & " UINT_MAX_STR ";",
              reg_names[inst->dst.reg],
              reg_names[inst->dst.reg], src_str(inst));
    break;

  case LOAD:
    emit_line("%s = mem[%s];", reg_names[inst->dst.reg], src_str(inst));
    break;

  case STORE:
    emit_line("mem[%s] = %s;", src_str(inst), reg_names[inst->dst.reg]);
    break;

  case PUTC:
    emit_line("putchar(%s);", src_str(inst));
    break;

  case GETC:
    emit_line("{ int _ = getchar(); %s = _ != EOF ? _ : 0; }",
              reg_names[inst->dst.reg]);
    break;

  case EXIT:
    emit_line("exit(0);");
    break;

  case DUMP:
    break;

  case EQ:
  case NE:
  case LT:
  case GT:
  case LE:
  case GE:
    emit_line("%s = %s;",
              reg_names[inst->dst.reg], cmp_str(inst, "1"));
    break;

  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
  case JMP:
    emit_line("if (%s) pc = %s - 1;",
              cmp_str(inst, "1"), value_str(&inst->jmp));
    break;

  default:
    error("oops");
  }
}

void target_c(Module* module) {
  c_init_state();

  int num_funcs = emit_chunked_main_loop(module->text,
                                         c_emit_func_prologue,
                                         c_emit_func_epilogue,
                                         c_emit_pc_change,
                                         c_emit_inst);

  emit_line("int main() {");
  inc_indent();

  Data* data = module->data;
  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      emit_line("mem[%d] = %d;", mp, data->v);
    }
  }

  emit_line("");
  emit_line("while (1) {");
  inc_indent();
  emit_line("switch (pc / %d | 0) {", CHUNKED_FUNC_SIZE);
  for (int i = 0; i < num_funcs; i++) {
    emit_line("case %d:", i);
    emit_line(" func%d();", i);
    emit_line(" break;");
  }
  emit_line("}");
  dec_indent();
  emit_line("}");

  emit_line("return 1;");
  dec_indent();
  emit_line("}");
}
