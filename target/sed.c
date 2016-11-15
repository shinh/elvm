#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include <ir/ir.h>
#include <target/util.h>

static void sed_init_state(Data* data) {
  for (int i = 0; i < 6; i++) {
    emit_line("s/$/%s=0 /", reg_names[i]);
  }
  emit_line("s/$/o= /");
  for (int mp = 0; data; data = data->next, mp++) {
    emit_line("s/$/m%x=%x /", mp, data->v);
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
    emit_line("s/^/0/");
    emit_line("s/.*\\(..\\)$/\\1/");
    emit_line("G");
    emit_line("s/^\\(..\\)\\n\\(.*o=[^ ]*\\)/\\2\\1/");
    emit_line("x");
    emit_line("s/.*//");
    break;

  case GETC: {
    break;
  }

  case EXIT:
    emit_line("bexit");
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

  emit_line(":exit");
  emit_line("s/.*//");
  emit_line("x");
  emit_line("s/.*o=\\([^ ]*\\).*/\\1/");
  emit_line(":out_loop");
  emit_line("/^$/bout_done");
  for (int i = 0; i < 256; i++) {
    printf("/^%x%x/{s/..//\nx\n", i / 16, i % 16);
    if (i == 10) {
      emit_line("p\ns/.*//\nx\n}");
    } else {
      char buf[6];
      buf[0] = i;
      buf[1] = 0;
      if (i == '/' || i == '\\' || i == '&') {
        buf[0] = '\\';
        buf[1] = i;
        buf[2] = 0;
      } else if (i == 0 || (i >= 0xc2 && i <= 0xfd)) {
        sprintf(buf, "\\x%x", i);
      }
      emit_line("s/$/%s/\nx\n}", buf);
    }
  }
  emit_line("bout_loop");
  emit_line(":out_done");
  emit_line("x");
  emit_line("p");
}
