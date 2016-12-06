#include <ir/ir.h>
#include <target/util.h>

#define GO_INT_TYPE "int32"

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

static void go_init_state(Data* data) {
  emit_line("copy(mem, []" GO_INT_TYPE "{");
  for (int mp = 0; data; data = data->next, mp++) {
    emit_line(" %d,", data->v);
  }
  emit_line("})");
}

void target_go(Module* module) {
  emit_line("package main");
  emit_line("import \"os\"");

  emit_line("func main() {");
  inc_indent();

  for (int i = 0; i < 7; i++) {
    emit_line("var %s " GO_INT_TYPE, reg_names[i]);
    emit_line("_ = %s", reg_names[i]);
  }
  emit_line("var buf [1]byte");
  emit_line("_ = buf");

  emit_line("mem := make([]" GO_INT_TYPE ", 1<<24)");
  go_init_state(module->data);

  emit_line("");
  emit_line("for {");
  inc_indent();

  emit_line("switch pc {");
  inc_indent();
  int prev_pc = -1;
  for (Inst* inst = module->text; inst; inst = inst->next) {
    if (prev_pc != inst->pc) {
      dec_indent();
      emit_line("");
      emit_line("case %d:", inst->pc);
      prev_pc = inst->pc;
      inc_indent();
    }
    go_emit_inst(inst);
  }
  // end of switch
  dec_indent();
  emit_line("}");
  emit_line("pc++");

  // end of for
  dec_indent();
  emit_line("}");

  // end of main
  dec_indent();
  emit_line("}");
}
