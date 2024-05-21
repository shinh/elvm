//                 Program Counter | A           | B           | C           | D           | BP          | SP          | mem[idx]
// Memory layout: -----------------+-------------+-------------+-------------+-------------+-------------+-------------+----------------------
//                 _%2^24          | _/4^12%2^24 | _/4^24%2^24 | _/4^36%2^24 | _/4^48%2^24 | _/4^60%2^24 | _/4^72%2^24 | _/2^(24*(7+idx))%2^24

#include <ir/ir.h>
#include <target/util.h>

static int ignore_inst = false;

static const char* acc_value_str(Value* v) {
  if (v->type == REG) {
    return format("_/4^%d%%2^24", 12 * (v->reg + 1));
  } else if (v->type == IMM) {
    return format("%d", v->imm);
  } else {
    error("invalid value");
  }
}

static const char* acc_mem_str(const char* idx) {
  return format("_/2^(24*(7+%s))%%2^24", idx);
}

static const char* acc_cmp_str(Inst* inst) {
  int op = normalize_cond(inst->op, 0);
  switch (op) {
    case JEQ:
      return format("0^((%s-%s)%%2^24)", acc_value_str(&inst->src), acc_value_str(&inst->dst));
    case JNE:
      return format("(1-0^((%s-%s)%%2^24))", acc_value_str(&inst->src), acc_value_str(&inst->dst));

    case JLT:
      return format("0^((%s+1)/(%s+1))", acc_value_str(&inst->dst), acc_value_str(&inst->src));
    case JGT:
      return format("0^((%s+1)/(%s+1))", acc_value_str(&inst->src), acc_value_str(&inst->dst));

    case JLE:
      return format("0^(%s/(%s+1))", acc_value_str(&inst->dst), acc_value_str(&inst->src));
    case JGE:
      return format("0^(%s/(%s+1))", acc_value_str(&inst->src), acc_value_str(&inst->dst));

    default:
      error("oops");
  }
}

static void acc_emit_func_prologue(int func_id) {
  emit_line("Count b while _*0^((_%%2^24-1)/%d-%d)^2 {", CHUNKED_FUNC_SIZE, func_id);
  inc_indent();
  emit_line("Count c while 0 {  # dummy");
  inc_indent();
}

static void acc_emit_func_epilogue(void) {
  emit_line("_+1");
  dec_indent();
  emit_line("}");
  dec_indent();
  emit_line("}");
}

static void acc_emit_pc_change(int pc) {
  ignore_inst = false;

  emit_line("_+1");
  dec_indent();
  emit_line("}");
  emit_line("");
  emit_line("Count c while 0^((_-%d)%%2^24) {", pc + 1);
  inc_indent();
}

static void acc_emit_inst(Inst* inst) {
  if (ignore_inst) return;

  switch (inst->op) {
  case MOV:
    emit_line("_+(%s-%s)*4^%d",
              acc_value_str(&inst->src),
              acc_value_str(&inst->dst),
              12 * (inst->dst.reg + 1));
    break;

  case ADD:
    emit_line("_+((%s+%s)%%2^24-%s)*4^%d",
              acc_value_str(&inst->src),
              acc_value_str(&inst->dst),
              acc_value_str(&inst->dst),
              12 * (inst->dst.reg + 1));
    break;

  case SUB:
    emit_line("_+((%s-%s)%%2^24-%s)*4^%d",
              acc_value_str(&inst->dst),
              acc_value_str(&inst->src),
              acc_value_str(&inst->dst),
              12 * (inst->dst.reg + 1));
    break;

  case LOAD:
    emit_line("_+(%s-%s)*4^%d",
              acc_mem_str(acc_value_str(&inst->src)),
              acc_value_str(&inst->dst),
              12 * (inst->dst.reg + 1));
    break;

  case STORE:
    emit_line("_+(%s-%s)*2^(24*(7+%s))",
              acc_value_str(&inst->dst),
              acc_mem_str(acc_value_str(&inst->src)),
              acc_value_str(&inst->src));
    break;

  case PUTC:
    emit_line("Write %s", acc_value_str(&inst->src));
    break;

  case GETC:
    emit_line("_+(N-%s)*4^%d",
              acc_value_str(&inst->dst),
              12 * (inst->dst.reg + 1));
    break;

  case EXIT:
    emit_line("-1");
    ignore_inst = true;
    break;

  case DUMP:
    break;

  case EQ:
  case NE:
  case LT:
  case GT:
  case LE:
  case GE:
    emit_line("_+(%s-%s)*4^%d",
              acc_cmp_str(inst),
              acc_value_str(&inst->dst),
              12 * (inst->dst.reg + 1));
    break;

  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
    emit_line("_+%s*(%s-_%%2^24)", acc_cmp_str(inst), acc_value_str(&inst->jmp));
    break;

  case JMP:
    emit_line("_-_%%2^24+%s", acc_value_str(&inst->jmp));
    break;

  default:
    break;
  }
}

void target_acc(Module* module) {
  emit_line("1");

  Data* data = module->data;
  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      emit_line("_+%d*4^%d", data->v, 12 * (mp + 7));
    }
  }

  emit_line("Count a while _ {");
  inc_indent();

  emit_chunked_main_loop(module->text,
                         acc_emit_func_prologue,
                         acc_emit_func_epilogue,
                         acc_emit_pc_change,
                         acc_emit_inst);

  dec_indent();
  emit_line("}");
}
