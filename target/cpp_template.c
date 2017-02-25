#include <ir/ir.h>
#include <target/util.h>
#include <target/cpp_template_lib.h>

static void cpp_template_emit_file_prologue(void) {
  emit_line("#include <cstdio>");
  emit_line("");
  emit_line(cpp_template_lib);
  emit_line("");
  emit_line("typedef enum { a, b, c, d, bp, sp } Reg_Name;");
}

static const char* cpp_template_op_str(Inst* inst) {
  const char * op_str;
  const char * src_str = inst->src.type == REG ? "reg" : "imm";
  switch (inst->op) {
  case MOV:
    op_str = "mov"; break;
  case ADD:
    op_str = "add"; break;
  case SUB:
    op_str = "sub"; break;
  case LOAD:
    op_str = "load"; break;
  case STORE:
    op_str = "store"; break;
  case PUTC:
    op_str = "putc"; break;
  case EQ:
  case NE:
  case LT:
  case GT:
  case LE:
  case GE:
    op_str = "cmp"; break;
  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
    op_str = "jcmp"; break;
  default:
    error("oops");
  }
  return format("%s_%s", op_str, src_str);
}


int reg_id = 0;
int mem_id = 0;
int buf_id = 0;
int exit_flag = 0;

static void cpp_template_emit_inst(Inst* inst) {
  switch (inst->op) {
  case MOV:
  case ADD:
  case SUB:
    emit_line("typedef %s<r%d, %s, %s> r%d;",
              cpp_template_op_str(inst),
              reg_id, reg_names[inst->dst.reg], src_str(inst), reg_id + 1);
    reg_id++;
    break;

  case LOAD:
    emit_line("typedef %s<r%d, m%d, %s, %s> r%d;",
              cpp_template_op_str(inst),
              reg_id, mem_id, reg_names[inst->dst.reg], src_str(inst), reg_id + 1);
    reg_id++;
    break;

  case STORE:
    emit_line("typedef %s<r%d, m%d, %s, %s> m%d;",
              cpp_template_op_str(inst),
              reg_id, mem_id, reg_names[inst->dst.reg], src_str(inst), mem_id + 1);
    mem_id++;
    break;

  case PUTC:
    emit_line("typedef %s<r%d, b%d,%s> b%d;",
              cpp_template_op_str(inst),
              reg_id, buf_id, src_str(inst), buf_id + 1);
    buf_id++;
    break;

  case GETC:
    emit_line("typedef getc_reg<r%d, %s> r%d;",
              reg_id, reg_names[inst->dst.reg], reg_id + 1);
    reg_id++;
    break;

  case EXIT:
    emit_line("typedef exit_inst<r%d> r%d;",
              reg_id, reg_id + 1);
    exit_flag = 1;
    reg_id++;
    return;

  case DUMP:
    break;

  case EQ:
  case NE:
  case LT:
  case GT:
  case LE:
  case GE:
    emit_line("typedef %s<r%d, %s, %s, %d> r%d;",
              cpp_template_op_str(inst),
              reg_id,
              reg_names[inst->dst.reg],
              src_str(inst),
              inst->op - EQ,
              reg_id + 1);
    reg_id++;
    break;

  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
    emit_line("typedef %s<r%d, %s - 1, %s, %s, %d> r%d;",
              cpp_template_op_str(inst),
              reg_id,
              value_str(&inst->jmp),
              reg_names[inst->dst.reg],
              src_str(inst),
              inst->op - JEQ,
              reg_id + 1);
    reg_id++;
    break;

  case JMP:
    emit_line("typedef jmp_%s<r%d, %s> r%d;",
              inst->jmp.type == REG ? "reg" : "imm",
              reg_id, value_str(&inst->jmp), reg_id + 1);
    reg_id++;
    break;

  default:
    error("oops");
  }
  return;
}

static void cpp_template_emit_pc_change(int pc) {
  emit_line("typedef inc_pc<r%d, m%d, b%d> result;", reg_id, mem_id, buf_id);
  dec_indent();
  emit_line("};");
  emit_line("");
  emit_line("template <typename r0, typename m0, typename b0>");
  emit_line("struct func_switch_impl<r0, m0, b0, %d> {", pc);
  inc_indent();
}

static void cpp_template_emit_func_epilogue() {
  emit_line("typedef inc_pc<r%d, m%d, b%d> result;", reg_id, mem_id, buf_id);
  dec_indent();
  emit_line("};");
  emit_line("");
  emit_line("template <typename env, int pc>");
  emit_line("struct func_switch : func_switch_impl<"
            "typename env::regs, typename env::mem, "
            "typename env::buf, pc>::result {};");
}

void cpp_template_emit_func_switch_impl(Inst* inst) {
  int prev_pc = 0;
  emit_line("template <typename r0, typename m0, typename b0, int pc>");
  emit_line("struct func_switch_impl { typedef inc_pc<r0, m0, b0> result; };");
  emit_line("");
  emit_line("template <typename r0, typename m0, typename b0>");
  emit_line("struct func_switch_impl<r0, m0, b0, 0> {");
  inc_indent();
  for (; inst; inst = inst->next) {
    if (prev_pc != inst->pc) {
      cpp_template_emit_pc_change(inst->pc);
      reg_id = 0;
      mem_id = 0;
      buf_id = 0;
      exit_flag = 0;
    }
    prev_pc = inst->pc;

    cpp_template_emit_inst(inst);
  }
  cpp_template_emit_func_epilogue();
}

static void cpp_template_emit_main_loop(void) {
  emit_line("template <typename env, bool, int>");
  emit_line("struct main_loop { typedef env result; };");
  emit_line("template <typename env, int pc>");
  emit_line("struct main_loop<env, false, pc> {");
  inc_indent();
  emit_line("typedef func_switch<env, pc> env2;");
  emit_line("typedef typename main_loop<env2, env2::regs::exit_flag, env2::regs::pc>::result result;");
  dec_indent();
  emit_line("};");
}

static void cpp_template_emit_calc_main(Data* data) {
  emit_line("struct calc_main {");
  inc_indent();
  emit_line("typedef init_memory<MEM_SIZE> mem0;");
  int mp;
  for (mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      emit_line("typedef store_value<mem%d, %d, %d> mem%d;", mp, mp, data->v, mp + 1);
    } else {
      emit_line("typedef mem%d mem%d;", mp, mp + 1);
    }
  }
  emit_line("typedef init_regs<0> regs;");
  emit_line(" typedef make_env<regs, mem%d, Nil> env;", mp);
  emit_line("typedef main_loop<env, false, 0>::result result;");
  dec_indent();
  emit_line("};");
}

void target_cpp_template(Module* module) {
  cpp_template_emit_file_prologue();
  emit_line("");

  cpp_template_emit_func_switch_impl(module->text);
  emit_line("");

  cpp_template_emit_main_loop();
  emit_line("");

  cpp_template_emit_calc_main(module->data);
  emit_line("");

  emit_line("int main() {");
  inc_indent();
  emit_line("typedef calc_main::result result;");
  emit_line("print_buffer<result::buf>::run();");
  dec_indent();
  emit_line("}");
}
