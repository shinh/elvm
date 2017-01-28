#include <ir/ir.h>
#include <target/util.h>

static void cs_emit_func_prologue(int func_id) {
  emit_line("");
  emit_line("private static void func%d() {", func_id);
  inc_indent();
  emit_line("while (%d <= pc && pc < %d) {",
            func_id * CHUNKED_FUNC_SIZE, (func_id + 1) * CHUNKED_FUNC_SIZE);
  inc_indent();
  emit_line("switch (pc) {");
  emit_line("case -1:  /* dummy */");
  inc_indent();
}

static void cs_emit_func_epilogue(void) {
  emit_line("break;");
  dec_indent();
  emit_line("}");
  emit_line("pc++;");
  dec_indent();
  emit_line("}");
  dec_indent();
  emit_line("}");
}

static void cs_emit_pc_change(int pc) {
  emit_line("break;");
  emit_line("");
  dec_indent();
  emit_line("case %d:", pc);
  inc_indent();
}

static void cs_emit_inst(Inst* inst) {
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
    emit_line("Console.Out.Write((char)%s);", src_str(inst));
    break;

  case GETC:
    emit_line("try { int _ = Console.In.Read(); "
              "  %s = _ == -1 ? 0 : _; }"
              "catch (Exception e) {}",
              reg_names[inst->dst.reg]);
    break;

  case EXIT:
    emit_line("Environment.Exit(0);");
    break;

  case DUMP:
    break;

  case EQ:
  case NE:
  case LT:
  case GT:
  case LE:
  case GE:
    emit_line("%s = %s ? 1 : 0;",
              reg_names[inst->dst.reg], cmp_str(inst, "true"));
    break;

  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
  case JMP:
    emit_line("if (%s) pc = %s - 1;",
              cmp_str(inst, "true"), value_str(&inst->jmp));
    break;

  default:
    error("oops");
  }
}

static int cs_init_state(Data* data) {
  int prev_mc = -1;
  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      int mc = mp / 1000;
      while (prev_mc != mc) {
        if (prev_mc != -1) {
          dec_indent();
          emit_line("}");
        }
        prev_mc++;
        emit_line("static void init%d() {", prev_mc);
        inc_indent();
      }
      emit_line("mem[%d] = %d;", mp, data->v);
    }
  }

  if (prev_mc != -1) {
    dec_indent();
    emit_line("}");
  }

  return prev_mc + 1;
}

void target_cs(Module* module) {
  emit_line("using System;");
  emit_line("namespace ConsoleApplication {");
  inc_indent();
  emit_line("public class Program {");
  inc_indent();
  for (int i = 0; i < 7; i++) {
    emit_line("static int %s;", reg_names[i]);
  }
  emit_line("static int[] mem = new int[1<<24];");

  int num_inits = cs_init_state(module->data);

  CHUNKED_FUNC_SIZE = 256;
  int num_funcs = emit_chunked_main_loop(module->text,
                                         cs_emit_func_prologue,
                                         cs_emit_func_epilogue,
                                         cs_emit_pc_change,
                                         cs_emit_inst);

  emit_line("public static void Main(string[] args) {");
  inc_indent();

  for (int i = 0; i < num_inits; i++) {
    emit_line("init%d();", i);
  }

  emit_line("");
  emit_line("while (true) {");
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

  dec_indent();
  emit_line("}");
  dec_indent();
  emit_line("}");
  dec_indent();
  emit_line("}");
}
