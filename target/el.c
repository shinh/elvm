#include <ir/ir.h>
#include <target/util.h>

static void init_state_el(Data* data) {
  emit_line("(load \"cl\" nil t)");
  for (int i = 0; i < 7; i++) {
    emit_line("(setq %s 0)", reg_names[i]);
  }
  emit_line("(setq mem (make-vector 16777216 0))");
  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      emit_line("(aset mem %d %d)", mp, data->v);
    }
  }
}

static void el_emit_func_prologue(int func_id) {
  emit_line("");
  emit_line("(defun elvm-func%d ()", func_id);
  inc_indent();
  emit_line("(while (and (<= %d pc) (< pc %d) elvm-running)",
            func_id * CHUNKED_FUNC_SIZE, (func_id + 1) * CHUNKED_FUNC_SIZE);
  inc_indent();
  emit_line("(cl-case pc");
  emit_line("(-1 nil");
  inc_indent();
}

static void el_emit_func_epilogue(void) {
  dec_indent();
  emit_line("))");
  emit_line("(setq pc (+ pc 1))");
  dec_indent();
  emit_line(")");
  dec_indent();
  emit_line(")");
}

static void el_emit_pc_change(int pc) {
  emit_line(")");
  emit_line("");
  dec_indent();
  emit_line("(%d", pc);
  inc_indent();
}

static void el_emit_inst(Inst* inst) {
  switch (inst->op) {
  case MOV:
    emit_line("(setq %s %s)", reg_names[inst->dst.reg], src_str(inst));
    break;

  case ADD:
    emit_line("(setq %s (logand (+ %s %s) " UINT_MAX_STR "))",
              reg_names[inst->dst.reg],
              reg_names[inst->dst.reg], src_str(inst));
    break;

  case SUB:
    emit_line("(setq %s (logand (- %s %s) " UINT_MAX_STR "))",
              reg_names[inst->dst.reg],
              reg_names[inst->dst.reg], src_str(inst));
    break;

  case LOAD:
    emit_line("(setq %s (aref mem %s))",
              reg_names[inst->dst.reg], src_str(inst));
    break;

  case STORE:
    emit_line("(aset mem %s %s)", src_str(inst), reg_names[inst->dst.reg]);
    break;

  case PUTC:
    emit_line("(putchar %s)", src_str(inst));
    break;

  case GETC:
    emit_line("(setq %s (getchar))",
              reg_names[inst->dst.reg]);
    break;

  case EXIT:
    emit_line("(setq elvm-running nil)");
    break;

  case DUMP:
    break;

  case EQ:
  case NE:
  case LT:
  case GT:
  case LE:
  case GE:
    error("cmp");
    emit_line("%s = (%s) | 0;",
              reg_names[inst->dst.reg], cmp_str(inst, "true"));
    break;

  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
    error("jcc");
    break;

  case JMP:
    emit_line("(setq pc (- %s 1))", value_str(&inst->jmp));
    break;

  default:
    error("oops");
  }
}

void target_el(Module* module) {
  init_state_el(module->data);

  emit_line("(defun getchar () 42)");
  emit_line("(defun putchar (c) (princ (char-to-string c)))");
  emit_line("(setq elvm-running t)");

  int num_funcs = emit_chunked_main_loop(module->text,
                                         el_emit_func_prologue,
                                         el_emit_func_epilogue,
                                         el_emit_pc_change,
                                         el_emit_inst);

  emit_line("");
  emit_line("(while elvm-running");
  inc_indent();
  emit_line("(cl-case (/ pc %d)", CHUNKED_FUNC_SIZE);
  for (int i = 0; i < num_funcs; i++) {
    emit_line("(%d (elvm-func%d))", i, i);
  }
  emit_line(")");
  dec_indent();
  emit_line(")");
}
