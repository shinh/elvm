#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ir/ir.h>
#include <target/util.h>

static const char* REG_NAMES[] = {
  "@a", "@b", "@c", "@d", "@bp", "@sp"
};

static void init_state(Data* data) {
  emit_line("@pc = 0");
  for (int i = 0; i < 6; i++) {
    emit_line("%s = 0", REG_NAMES[i]);
  }
  emit_line("@mem = [0] * (1 << 24)");
  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      emit_line("@mem[%d] = %d", mp, data->v);
    }
  }
}

static const char* value(Value* v) {
  if (v->type == REG) {
    return REG_NAMES[v->reg];
  } else if (v->type == IMM) {
    return format("%d", v->imm);
  } else {
    error("invalid value");
  }
}

static const char* src(Inst* inst) {
  return value(&inst->src);
}

static const char* cmp(Inst* inst) {
  int op = inst->op;
  if (op >= 16)
    op -= 8;
  const char* op_str;
  switch (op) {
    case JEQ:
      op_str = "=="; break;
    case JNE:
      op_str = "!="; break;
    case JLT:
      op_str = "<"; break;
    case JGT:
      op_str = ">"; break;
    case JLE:
      op_str = "<="; break;
    case JGE:
      op_str = ">="; break;
    case JMP:
      return "true";
    default:
      error("oops");
  }
  return format("%s %s %s", REG_NAMES[inst->dst.reg], op_str, src(inst));
}

void target_rb(Module* module) {
  init_state(module->data);
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
        emit_line("%s = %s", REG_NAMES[inst->dst.reg], src(inst));
        break;

      case ADD:
        emit_line("%s = (%s + %s) & " UINT_MAX_STR,
                  REG_NAMES[inst->dst.reg],
                  REG_NAMES[inst->dst.reg], src(inst));
        break;

      case SUB:
        emit_line("%s = (%s - %s) & " UINT_MAX_STR,
                  REG_NAMES[inst->dst.reg],
                  REG_NAMES[inst->dst.reg], src(inst));
        break;

      case LOAD:
        emit_line("%s = @mem[%s]", REG_NAMES[inst->dst.reg], src(inst));
        break;

      case STORE:
        emit_line("@mem[%s] = %s", src(inst), REG_NAMES[inst->dst.reg]);
        break;

      case PUTC:
        emit_line("putc %s", src(inst));
        break;

      case GETC:
        emit_line("c = STDIN.getc; %s = c ? c.ord : 0",
                  REG_NAMES[inst->dst.reg]);
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
        emit_line("%s = %s ? 1 : 0", REG_NAMES[inst->dst.reg], cmp(inst));
        break;

      case JEQ:
      case JNE:
      case JLT:
      case JGT:
      case JLE:
      case JGE:
      case JMP:
        emit_line("%s && @pc = %s - 1", cmp(inst), value(&inst->jmp));
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
