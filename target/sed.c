#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include <ir/ir.h>
#include <target/util.h>

static void sed_init_state(Data* data) {
  for (int i = 0; i < 6; i++) {
    emit_line("s/$/%s=0\\n/", reg_names[i]);
  }
  for (int mp = 0; data; data = data->next, mp++) {
    emit_line("s/$/m%x=%x\\n/", mp, data->v);
  }
  emit_line("x");
}

static void sed_emit_value(Value* v) {
  if (v->type == REG) {
    assert(0);
  } else {
    emit_line("s/$/%x/", v->imm);
  }
}

static void sed_emit_src(Inst* inst) {
  sed_emit_value(&inst->src);
}

static void sed_emit_inst(Inst* inst) {
  switch (inst->op) {
  case MOV:
    break;

  case ADD:
    break;

  case SUB:
    break;

  case LOAD:
    break;

  case STORE:
    break;

  case PUTC:
    sed_emit_src(inst);
    emit_line("s/.*\(..\)$/\1/");
    emit_line("s/^0*//");
    break;

  case GETC: {
    break;
  }

  case EXIT:
    break;

  case DUMP:
    break;

  case EQ:
  case NE:
  case LT:
  case GT:
  case LE:
  case GE:
    break;

  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
    break;

  case JMP:
    break;

  default:
    error("oops");
  }
}

void target_sed(Module* module) {
  sed_init_state(module->data);

  int prev_pc = -1;
  for (Inst* inst = module->text; inst; inst = inst->next) {
    if (prev_pc != inst->pc) {
      // TODO
    }
    prev_pc = inst->pc;
    sed_emit_inst(inst);
  }
}
