#include <ir/ir.h>
#include <target/util.h>

static void init_state_cl(Data* data) {
  emit_line("(declaim (optimize (debug 0) (safety 0) (speed 3)))");
  emit_line("(defpackage #:elvm-compiled (:use #:cl) (:export #:elvm-main))");
  emit_line("(in-package #:elvm-compiled)");
  for (int i = 0; i < 7; i++) {
    emit_line("(defvar %s 0)", reg_names[i]);
  }
  emit_line("(declaim (type fixnum");
  for (int i = 0; i < 7; i++) {
    emit_line(" %s", reg_names[i]);
  }
  emit_line("))");
  emit_line("(defvar mem nil)");
  emit_line("(defparameter mem-init '(");
  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      emit_line("  (%d . %d)", mp, data->v);
    }
  }
  emit_line("))");
  emit_line("(defvar elvm-running nil)");
  emit_line("(defvar elvm-input nil)");
  emit_line("(defvar elvm-output nil)");
}

static void cl_emit_func_prologue(int func_id) {
  emit_line("");
  emit_line("(defun elvm-func%d ()", func_id);
  inc_indent();
  emit_line("(loop while (and (<= %d pc) (< pc %d) elvm-running) do",
            func_id * CHUNKED_FUNC_SIZE, (func_id + 1) * CHUNKED_FUNC_SIZE);
  inc_indent();
  emit_line("(case pc");
  emit_line("(-1 nil");
  inc_indent();
}

static void cl_emit_func_epilogue(void) {
  dec_indent();
  emit_line("))");
  emit_line("(setq pc (+ pc 1))");
  dec_indent();
  emit_line(")");
  dec_indent();
  emit_line(")");
}

static void cl_emit_pc_change(int pc) {
  emit_line(")");
  emit_line("");
  dec_indent();
  emit_line("(%d", pc);
  inc_indent();
}

static const char* cl_cmp_str(Inst* inst) {
  int op = normalize_cond(inst->op, false);
  const char* fmt;
  switch (op) {
    case JEQ: fmt = "(= %s %s)"; break;
    case JNE: fmt = "(/= %s %s)"; break;
    case JLT: fmt = "(< %s %s)"; break;
    case JGT: fmt = "(> %s %s)"; break;
    case JLE: fmt = "(<= %s %s)"; break;
    case JGE: fmt = "(>= %s %s)"; break;
    default:
      error("oops");
  }
  return format(fmt, reg_names[inst->dst.reg], src_str(inst));
}

static void cl_emit_inst(Inst* inst) {
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
    emit_line("(setf (aref mem %s) %s)", src_str(inst), reg_names[inst->dst.reg]);
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
              reg_names[inst->dst.reg], cl_cmp_str(inst));
    break;

  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
    emit_line("(if %s (setq pc (- %s 1)))",
              cl_cmp_str(inst), value_str(&inst->jmp));
    break;

  case JMP:
    emit_line("(setq pc (- %s 1))", value_str(&inst->jmp));
    break;

  default:
    error("oops");
  }
}

void target_cl(Module* module) {
  init_state_cl(module->data);
  emit_line("(defun getchar ()");
  emit_line(" (if (listen elvm-input)");
  emit_line("  (char-code (read-char elvm-input))");
  emit_line("  0))");
  emit_line("(defun putchar (c)");
  emit_line(" (princ (code-char c) elvm-output))");

  int num_funcs = emit_chunked_main_loop(module->text,
                                         cl_emit_func_prologue,
                                         cl_emit_func_epilogue,
                                         cl_emit_pc_change,
                                         cl_emit_inst);

  emit_line("(defun elvm-main (&optional (input-stream *standard-input*) (output-stream *standard-output*))");
  inc_indent();
  emit_line("(setq elvm-input input-stream)");
  emit_line("(setq elvm-output output-stream)");
  for (int i = 0; i < 7; i++) {
    emit_line("(setq %s 0)", reg_names[i]);
  }
  emit_line("(setq mem (make-array 16777216 :element-type 'fixnum :initial-element 0))");
  emit_line("(dolist (p mem-init)");
  emit_line(" (setf (aref mem (car p)) (cdr p)))");
  emit_line("(setq elvm-running t)");
  emit_line("(loop while elvm-running do");
  inc_indent();
  emit_line("(case (truncate pc %d)", CHUNKED_FUNC_SIZE);
  inc_indent();
  for (int i = 0; i < num_funcs; i++) {
    emit_line("(%d (elvm-func%d))", i, i);
  }
  emit_line(")))");
  dec_indent();
  dec_indent();
  dec_indent();
  emit_line("(elvm-main)");
}
