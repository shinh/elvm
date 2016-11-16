#include <ir/ir.h>
#include <target/util.h>

size_t BUF_SIZE = 10000;

static void cpp_defs(void) {
  emit_line("#include <cstdio>");
  emit_line("#include <utility>");
  emit_line("");
  emit_line("const size_t BUF_SIZE = %d;", BUF_SIZE);
  emit_line("");
  emit_line("constexpr char input[] =");
  emit_line("#include \"input.txt\"");
  emit_line(";");
  emit_line("");
  emit_line("struct buffer {");
  emit_line(" unsigned int size;");
  emit_line(" unsigned int b[BUF_SIZE];");
  emit_line("};");
  emit_line("template <size_t... I>");
  emit_line("constexpr buffer make_buf_impl(unsigned int size, unsigned int* buf, std::index_sequence<I...>) {");
  emit_line(" return buffer{size, {buf[I]...}};");
  emit_line("}");
  emit_line("constexpr buffer make_buf(unsigned int size, unsigned int* buf) {");
  emit_line(" return make_buf_impl(size, buf, std::make_index_sequence<BUF_SIZE>{});");
  emit_line("}");
}

static void cpp_init_state(Data* data) {
  for (int i = 0; i < 7; i++) {
    emit_line("unsigned int %s = 0;", reg_names[i]);
  }

  emit_line("unsigned int i_cur = 0;");
  emit_line("unsigned int o_cur = 0;");
  emit_line("unsigned int mem[1<<24] = {0};");
  emit_line("unsigned int buf[BUF_SIZE] = {0};");

  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      emit_line("mem[%d] = %d;", mp, data->v);
    }
  }
}

static void cpp_emit_prologue(void) {
  emit_line("");
  emit_line("while (1) {");
  inc_indent();
  emit_line("switch (pc) {");
  emit_line("case 0:");
  inc_indent();
}

static void cpp_emit_epilogue(void) {
  dec_indent();
  emit_line("}");
  emit_line("pc++;");
  dec_indent();
  emit_line("}");
  emit_line("return make_buf(o_cur, buf);");
  dec_indent();
  emit_line("}");
}

static void cpp_emit_pc_change(int pc) {
  emit_line("break;");
  emit_line("");
  dec_indent();
  emit_line("case %d:", pc);
  inc_indent();
}

static void cpp_emit_inst(Inst* inst) {
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
    emit_line("buf[o_cur++] = %s;", src_str(inst));
    break;

  case GETC:
    emit_line("%s = (input[i_cur] ? input[i_cur++] : 0);",
              reg_names[inst->dst.reg]);
    break;

  case EXIT:
    emit_line("return make_buf(o_cur, buf);");
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

void cpp_emit_main_loop(Inst* inst) {
  int prev_pc = 0;
  for (; inst; inst = inst->next) {
    if (prev_pc != inst->pc) {
      cpp_emit_pc_change(inst->pc);
    }
    prev_pc = inst->pc;
    cpp_emit_inst(inst);
  }
}

void target_cpp(Module* module) {
  cpp_defs();
  emit_line("");
  emit_line("constexpr buffer constexpr_main() {");
  inc_indent();

  cpp_init_state(module->data);
  cpp_emit_prologue();
  cpp_emit_main_loop(module->text);
  cpp_emit_epilogue();

  emit_line("");
  emit_line("int main() {");
  emit_line(" constexpr buffer buf = constexpr_main();");
  emit_line(" constexpr unsigned int output_size = buf.size;");
  emit_line("");
  emit_line(" for(int i = 0; i < output_size; ++i) {");
  emit_line("  putchar(buf.b[i]);");
  emit_line(" }");
  emit_line("}");
}
