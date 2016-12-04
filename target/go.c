#include <ir/ir.h>
#include <target/util.h>

#define GO_INT_TYPE "int32"

static void go_emit_func_prologue(int func_id) {
  emit_line("");
  emit_line("func func%d() {", func_id);
  inc_indent();
  emit_line("for %d <= pc && pc < %d {",
            func_id * CHUNKED_FUNC_SIZE, (func_id + 1) * CHUNKED_FUNC_SIZE);
  inc_indent();
  emit_line("switch pc {");
  emit_line("case -1:  /* dummy */");
  inc_indent();
}

static void go_emit_func_epilogue(void) {
  dec_indent();
  emit_line("}");
  emit_line("pc++;");
  dec_indent();
  emit_line("}");
  dec_indent();
  emit_line("}");
}

static void go_emit_pc_change(int pc) {
  emit_line("");
  dec_indent();
  emit_line("case %d:", pc);
  inc_indent();
}

static void go_emit_inst(Inst* inst) {
  switch (inst->op) {
  case MOV:
    emit_line("%s = %s", reg_names[inst->dst.reg], src_str(inst));
    break;

  case ADD:
    emit_line("%s = (%s + %s) & " UINT_MAX_STR,
              reg_names[inst->dst.reg],
              reg_names[inst->dst.reg], src_str(inst));
    break;

  case SUB:
    emit_line("%s = (%s - %s) & " UINT_MAX_STR,
              reg_names[inst->dst.reg],
              reg_names[inst->dst.reg], src_str(inst));
    break;

  case LOAD:
    emit_line("%s = mem[%s]", reg_names[inst->dst.reg], src_str(inst));
    break;

  case STORE:
    emit_line("mem[%s] = %s", src_str(inst), reg_names[inst->dst.reg]);
    break;

  case PUTC:
    emit_line("buf[0] = byte(%s); os.Stdout.Write(buf[:])", src_str(inst));
    break;

  case GETC:
      emit_line("if n, err := os.Stdin.Read(buf[:]); n != 0 && err == nil { %s = " GO_INT_TYPE "(buf[0]) } else { %s = 0 }",
                reg_names[inst->dst.reg], reg_names[inst->dst.reg]);
    break;

  case EXIT:
    emit_line("os.Exit(0)");
    break;

  case DUMP:
    break;

  case EQ:
  case NE:
  case LT:
  case GT:
  case LE:
  case GE:
    emit_line("if %s { %s = 1 } else { %s = 0 }",
              cmp_str(inst, "true"), reg_names[inst->dst.reg], reg_names[inst->dst.reg]);
    break;

  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
  case JMP:
    emit_line("if %s { pc = %s - 1 }",
              cmp_str(inst, "true"), value_str(&inst->jmp));
    break;

  default:
    error("oops");
  }
}

static int go_init_state(Data* data) {
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
        emit_line("func init%d() {", prev_mc);
        inc_indent();
      }
      emit_line("mem[%d] = %d", mp, data->v);
    }
  }

  if (prev_mc != -1) {
    dec_indent();
    emit_line("}");
  }

  return prev_mc + 1;
}

void target_go(Module* module) {
  emit_line("package main");
  emit_line("import \"os\"");
  for (int i = 0; i < 7; i++) {
    emit_line("var %s " GO_INT_TYPE, reg_names[i]);
  }
  emit_line("var mem []" GO_INT_TYPE);
  emit_line("var buf [1]byte");

  int num_inits = go_init_state(module->data);

  CHUNKED_FUNC_SIZE = 256;
  int num_funcs = emit_chunked_main_loop(module->text,
                                         go_emit_func_prologue,
                                         go_emit_func_epilogue,
                                         go_emit_pc_change,
                                         go_emit_inst);

  emit_line("func main() {");
  inc_indent();

  emit_line("mem = make([]" GO_INT_TYPE ", 1<<24)");
  for (int i = 0; i < num_inits; i++) {
    emit_line("init%d()", i);
  }

  emit_line("");
  emit_line("for {");
  inc_indent();
  emit_line("switch pc / %d {", CHUNKED_FUNC_SIZE);
  for (int i = 0; i < num_funcs; i++) {
    emit_line("case %d:", i);
    emit_line(" func%d()", i);
  }
  emit_line("}");
  dec_indent();
  emit_line("}");

  dec_indent();
  emit_line("}");
}
