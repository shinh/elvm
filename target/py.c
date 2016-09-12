#include <ir/ir.h>
#include <target/util.h>

static void init_state(Data* data) {
  emit_line("import sys");
  for (int i = 0; i < 7; i++) {
    emit_line("%s = 0", reg_names[i]);
  }
  emit_line("mem = [0] * (1 << 24)");
  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      emit_line("mem[%d] = %d", mp, data->v);
    }
  }
}

void target_py(Module* module) {
  init_state(module->data);

  emit_line("while True:");
  inc_indent();
  emit_line("if False:");
  inc_indent();
  emit_line("pass");

  int prev_pc = -1;
  for (Inst* inst = module->text; inst; inst = inst->next) {
    if (prev_pc != inst->pc) {
      emit_line("");
      dec_indent();
      emit_line("elif pc == %d:", inst->pc);
      inc_indent();
    }
    prev_pc = inst->pc;

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
        emit_line("sys.stdout.write(chr(%s))", src_str(inst));
        break;

      case GETC:
        emit_line("c = sys.stdin.read(1); %s = ord(c) if c else 0",
                  reg_names[inst->dst.reg]);
        break;

      case EXIT:
        emit_line("sys.exit(0)");
        break;

      case DUMP:
        break;

      case EQ:
      case NE:
      case LT:
      case GT:
      case LE:
      case GE:
        emit_line("%s = int(%s)",
                  reg_names[inst->dst.reg], cmp_str(inst, "True"));
        break;

      case JEQ:
      case JNE:
      case JLT:
      case JGT:
      case JLE:
      case JGE:
      case JMP:
        emit_line("if %s: pc = %s - 1",
                  cmp_str(inst, "True"), value_str(&inst->jmp));
        break;

      default:
        error("oops");
    }
  }

  dec_indent();
  emit_line("pc += 1");
  dec_indent();
}
