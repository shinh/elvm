#include <ir/ir.h>
#include <target/util.h>

static const char* REG_NAMES[] = {
  "@a", "@b", "@c", "@d", "@bp", "@sp", "@pc"
};

static void init_state_rb(Data* data) {
  reg_names = REG_NAMES;
  for (int i = 0; i < 7; i++) {
    emit_line("%s = 0", reg_names[i]);
  }
  emit_line("@mem = [0] * (1 << 24)");
  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      emit_line("@mem[%d] = %d", mp, data->v);
    }
  }
}

void target_rb(Module* module) {
  init_state_rb(module->data);
  emit_line("");

  static const int FUNC_SIZE = 1024;

  int prev_pc = -1;
  int prev_page = -1;
  for (Inst* inst = module->text; inst; inst = inst->next) {
    int page = inst->pc / FUNC_SIZE;
    if (prev_pc != inst->pc) {
      if (prev_page != page) {
        if (prev_page >= 0) {
          dec_indent();
          emit_line("end");
          emit_line("@pc += 1");
          dec_indent();
          emit_line("end");
          dec_indent();
          emit_line("end");
        }
        emit_line("");
        emit_line("def page%d", page);
        inc_indent();
        emit_line("while %d <= @pc && @pc < %d",
                  page * FUNC_SIZE, (page + 1) * FUNC_SIZE);
        inc_indent();
        emit_line("case @pc");
        inc_indent();
      }

      emit_line("");
      dec_indent();
      emit_line("when %d", inst->pc);
      inc_indent();
    }
    prev_pc = inst->pc;
    prev_page = page;

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
        emit_line("%s = @mem[%s]", reg_names[inst->dst.reg], src_str(inst));
        break;

      case STORE:
        emit_line("@mem[%s] = %s", src_str(inst), reg_names[inst->dst.reg]);
        break;

      case PUTC:
        emit_line("putc %s", src_str(inst));
        break;

      case GETC:
        emit_line("c = STDIN.getc; %s = c ? c.ord : 0",
                  reg_names[inst->dst.reg]);
        break;

      case EXIT:
        emit_line("exit");
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
                  reg_names[inst->dst.reg], cmp_str(inst, "true"));
        break;

      case JEQ:
      case JNE:
      case JLT:
      case JGT:
      case JLE:
      case JGE:
      case JMP:
        emit_line("%s && @pc = %s - 1",
                  cmp_str(inst, "true"), value_str(&inst->jmp));
        break;

      default:
        error("oops");
    }
  }

  dec_indent();
  emit_line("end");
  emit_line("@pc += 1");
  dec_indent();
  emit_line("end");
  dec_indent();
  emit_line("end");

  emit_line("");
  emit_line("while true");
  inc_indent();
  emit_line("case @pc / %d", FUNC_SIZE);
  for (int i = 0; i <= prev_page; i++) {
    emit_line("when %d", i);
    emit_line(" page%d", i);
  }
  emit_line("end");
  dec_indent();
  emit_line("end");
}
