#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ir/ir.h>
#include <target/util.h>

static const char* REG_NAMES[] = {
  "a", "b", "c", "d", "bp", "sp"
};

static void init_state(Data* data) {
  emit_line("var sys = require('sys');");

  emit_line("var input = null;");
  emit_line("var ip = 0;");
  emit_line("var getchar = function() {");
  emit_line(" if (input === null)");
  emit_line("  input = require('fs').readFileSync('/dev/stdin');");
  emit_line(" return input[ip++] | 0;");
  emit_line("}");

  emit_line("var pc = 0;");
  for (int i = 0; i < 6; i++) {
    emit_line("var %s = 0;", REG_NAMES[i]);
  }
  emit_line("var mem = Int32Array(1 << 24);");
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
      return "true";
    default:
      error("oops");
  }
  return format("%s %s %s", REG_NAMES[inst->dst.reg], op_str, src(inst));
}

void target_js(Module* module) {
  init_state(module->data);

  emit_line("var running = true;");
  emit_line("while (running) {");
  inc_indent();
  emit_line("switch (pc) {");
  emit_line("case -1:  // dummy");
  inc_indent();

  int prev_pc = -1;
  for (Inst* inst = module->text; inst; inst = inst->next) {
    if (prev_pc != inst->pc) {
      emit_line("break;");
      emit_line("");
      dec_indent();
      emit_line("case %d:", inst->pc);
      inc_indent();
    }
    prev_pc = inst->pc;

    switch (inst->op) {
      case MOV:
        emit_line("%s = %s", REG_NAMES[inst->dst.reg], src(inst));
        break;

      case ADD:
        emit_line("%s = (%s + %s) & " UINT_MAX_STR ";",
                  REG_NAMES[inst->dst.reg],
                  REG_NAMES[inst->dst.reg], src(inst));
        break;

      case SUB:
        emit_line("%s = (%s - %s) & " UINT_MAX_STR ";",
                  REG_NAMES[inst->dst.reg],
                  REG_NAMES[inst->dst.reg], src(inst));
        break;

      case LOAD:
        emit_line("%s = mem[%s];", REG_NAMES[inst->dst.reg], src(inst));
        break;

      case STORE:
        emit_line("mem[%s] = %s;", src(inst), REG_NAMES[inst->dst.reg]);
        break;

      case PUTC:
        emit_line("sys.print(String.fromCharCode((%s) & 255));",
                  src(inst));
        break;

      case GETC:
        emit_line("%s = getchar();",
                  REG_NAMES[inst->dst.reg]);
        break;

      case EXIT:
        emit_line("running = false; break;");
        break;

      case DUMP:
        break;

      case EQ:
      case NE:
      case LT:
      case GT:
      case LE:
      case GE:
        emit_line("%s = (%s) | 0;", REG_NAMES[inst->dst.reg], cmp(inst));
        break;

      case JEQ:
      case JNE:
      case JLT:
      case JGT:
      case JLE:
      case JGE:
      case JMP:
        emit_line("if (%s) pc = %s - 1;", cmp(inst), value(&inst->jmp));
        break;

      default:
        error("oops");
    }
  }

  dec_indent();
  emit_line("}");
  emit_line("pc += 1;");
  dec_indent();
  emit_line("}");
}
