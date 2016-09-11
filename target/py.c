#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ir/ir.h>
#include <target/util.h>

static const char* REG_NAMES[] = {
  "a", "b", "c", "d", "bp", "sp"
};

static void init_state(Data* data) {
  emit_line("import sys");
  emit_line("pc = 0");
  for (int i = 0; i < 6; i++) {
    emit_line("%s = 0", REG_NAMES[i]);
  }
  emit_line("mem = [0] * (1 << 24)");
  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      emit_line("mem[%d] = %d", mp, data->v);
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
      return "True";
    default:
      error("oops");
  }
  return format("%s %s %s", REG_NAMES[inst->dst.reg], op_str, src(inst));
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
        emit_line("%s = mem[%s]", REG_NAMES[inst->dst.reg], src(inst));
        break;

      case STORE:
        emit_line("mem[%s] = %s", src(inst), REG_NAMES[inst->dst.reg]);
        break;

      case PUTC:
        emit_line("sys.stdout.write(chr(%s))", src(inst));
        break;

      case GETC:
        emit_line("c = sys.stdin.read(1); %s = ord(c) if c else 0",
                  REG_NAMES[inst->dst.reg]);
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
        emit_line("%s = int(%s)", REG_NAMES[inst->dst.reg], cmp(inst));
        break;

      case JEQ:
      case JNE:
      case JLT:
      case JGT:
      case JLE:
      case JGE:
      case JMP:
        emit_line("if %s: pc = %s - 1", cmp(inst), value(&inst->jmp));
        break;

      default:
        error("oops");
    }
  }

  dec_indent();
  emit_line("pc += 1");
  dec_indent();
}
