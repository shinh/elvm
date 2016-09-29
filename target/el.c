#include <ir/ir.h>
#include <target/util.h>

static void init_state_el(Data* data) {
  emit_line("(setq elvm-main (lambda ()");
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

static const char* el_cmp_str(Inst* inst) {
  int op = normalize_cond(inst->op, false);
  const char* fmt;
  switch (op) {
    case JEQ: fmt = "(eq %s %s)"; break;
    case JNE: fmt = "(not (eq %s %s))"; break;
    case JLT: fmt = "(< %s %s)"; break;
    case JGT: fmt = "(> %s %s)"; break;
    case JLE: fmt = "(<= %s %s)"; break;
    case JGE: fmt = "(>= %s %s)"; break;
    default:
      error("oops");
  }
  return format(fmt, reg_names[inst->dst.reg], src_str(inst));
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
    emit_line("(setq %s (if %s 1 0))",
              reg_names[inst->dst.reg], el_cmp_str(inst));
    break;

  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
    emit_line("(if %s (setq pc (- %s 1)))",
              el_cmp_str(inst), value_str(&inst->jmp));
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

  emit_line("))");

  emit_line("(defun getchar ()");
  emit_line(" (if (eq (length elvm-input) 0) 0");
  emit_line("  (setq r (string-to-char elvm-input))");
  emit_line("  (setq elvm-input (substring elvm-input 1))");
  emit_line("  r))");
  emit_line("(if noninteractive (progn");
  emit_line(" (setq elvm-input");
  emit_line("  (with-temp-buffer");
  emit_line("   (insert-file-contents \"/dev/stdin\")");
  emit_line("   (buffer-string)))");
  emit_line(" (defun putchar (c) (princ (char-to-string c)))");
  emit_line(" (funcall elvm-main)))");
}
