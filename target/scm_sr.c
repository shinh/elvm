#include <ir/ir.h>
#include <target/util.h>
#include <target/scm_sr_lib.h>

static void scm_sr_emit_file_prologue(void) {
  emit_line(scm_sr_lib);
}

static void scm_sr_emit_file_epilogue(void) {
  emit_line(";;; Load Input");
  emit_line("(load \"./input.scm\")");
  emit_line(";;; Run VM!");
  emit_line("(ck () (run-vm! '\"load\" (zero!) (init-reg!)");
  emit_line("                (dmem-init!) (imem-init!)");
  emit_line("                (input!) '()))");
}

static void scm_sr_emit_func_prologue(int i) {
  emit_line("(define-syntax func-impl%d!", i);
  inc_indent();
  emit_line("(syntax-rules (quote) ((_ s) (ck s '(");
  inc_indent();
}

static void scm_sr_emit_func_epilogue() {
  dec_indent();
  emit_line(")))))");
  dec_indent();
}

static char* scm_sr_op_str(Inst* inst) {
  switch (inst->op) {
  case MOV:
    return "\"MOV\"";
  case ADD:
    return "\"ADD\"";
  case SUB:
    return "\"SUB\"";
  case LOAD:
    return "\"LOAD\"";
  case STORE:
    return "\"STORE\"";
  case PUTC:
    return "\"PUTC\"";
  case EQ:
  case JEQ:
    return "\"EQ\"";
  case NE:
  case JNE:
    return "\"NE\"";
  case LT:
  case JLT:
    return "\"LT\"";
  case GT:
  case JGT:
    return "\"GT\"";
  case LE:
  case JLE:
    return "\"LE\"";
  case GE:
  case JGE:
    return "\"GE\"";
  default:
    error("oopsa %d", inst->op);
  }
  return "BAD_INSTRUCTION";
}

static char* scm_sr_imm_str(int imm, int n) {
  if (n == 1) {
    return format("%d", imm & 1);
  } else {
    return format("%d %s", imm & 1, scm_sr_imm_str(imm >> 1, n - 1));
  }
}

static char* scm_sr_jmp_str(Inst* inst) {
  if (inst->jmp.type == REG) {
    return format("(\"REG\" %s)", reg_names[inst->jmp.reg]);
  } else {
    return format("(\"IMM\" (%s))", scm_sr_imm_str(inst->jmp.imm, 24));
  }
}

static char* scm_sr_src_str(Inst* inst) {
  if (inst->src.type == REG) {
    return format("(\"REG\" %s)", reg_names[inst->src.reg]);
  } else {
    return format("(\"IMM\" (%s))", scm_sr_imm_str(inst->src.imm, 24));
  }
}

static void scm_sr_emit_inst(Inst* inst) {
  switch (inst->op) {
  case MOV:
  case ADD:
  case SUB:
  case LOAD:
  case STORE:
    emit_line("(%s %s %s)",
              scm_sr_op_str(inst),
	      reg_names[inst->dst.reg],
	      scm_sr_src_str(inst));
    break;

  case PUTC:
    emit_line("(\"PUTC\" %s)",
	      scm_sr_src_str(inst));
    break;

  case GETC:
    emit_line("(\"GETC\" %s)",
	      reg_names[inst->dst.reg]);
    break;

  case EXIT:
    emit_line("(\"EXIT\")");
    return;

  case DUMP:
    break;

  case EQ:
  case NE:
  case LT:
  case GT:
  case LE:
  case GE:
    emit_line("(\"CMP\" %s %s %s)",
              scm_sr_op_str(inst),
	      reg_names[inst->dst.reg],
	      scm_sr_src_str(inst));
    break;

  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
    emit_line("(\"JCOND\" %s %s %s %s)",
              scm_sr_op_str(inst),
              scm_sr_jmp_str(inst),
	      reg_names[inst->dst.reg],
	      scm_sr_src_str(inst));
    break;

  case JMP:
    emit_line("(\"JMP\" %s)",
              scm_sr_jmp_str(inst));
    break;

  default:
    error("oops");
  }
  return;
}

static void scm_sr_emit_func_impl(Inst* inst) {
  int prev_pc = 0;
  scm_sr_emit_func_prologue(0);
  for (; inst; inst = inst->next) {
    if (prev_pc != inst->pc) {
      scm_sr_emit_func_epilogue();
      emit_line("");
      scm_sr_emit_func_prologue(inst->pc);
    }
    prev_pc = inst->pc;

    scm_sr_emit_inst(inst);
  }
  scm_sr_emit_func_epilogue();
}

static int scm_sc_count_pc(Inst* inst) {
  int max_pc = 0;
  for (; inst; inst = inst->next) {
    max_pc = (max_pc > inst->pc)?max_pc:inst->pc;
  }
  return max_pc;
}

static void scm_sr_emit_inst_mem_rec(int max_pc, int from, int to) {
  if (max_pc < from) {
    emit_line("'()");
  } else if (from + 1 >= to) {
    emit_line("(func-impl%d!)", from);
  } else {
    int mid = (from + to)/2;
    emit_line("(cons!");
    inc_indent();
    scm_sr_emit_inst_mem_rec(max_pc, from, mid);
    scm_sr_emit_inst_mem_rec(max_pc, mid, to);
    dec_indent();
    emit_line(")");
  }
}

static void scm_sr_emit_inst_mem(int max_pc) {
  emit_line("(define-syntax imem-init!");
  inc_indent();
  emit_line("(syntax-rules (quote) ((_ s) (ck s");
  inc_indent();
  scm_sr_emit_inst_mem_rec(max_pc, 0, 1 << 24);
  dec_indent();
  emit_line("))))");
  dec_indent();
}

static Data* scm_sr_take_data(Data* data, int n) {
  int i = 0;
  for (; i < n && data; i++) {
    data = data->next;
  }
  return data;
}

static void scm_sr_emit_data_mem_rec(Data* data, int from, int to) {
  if (data == NULL) {
    emit_line("()");
  } else if (from + 1 >= to) {
    emit_line("(%s)", scm_sr_imm_str(data->v, 24));
  } else {
    int mid = (from + to)/2;
    emit_line("(");
    inc_indent();
    scm_sr_emit_data_mem_rec(data, from, mid);
    emit_line(".");
    scm_sr_emit_data_mem_rec(scm_sr_take_data(data, mid - from), mid, to);
    dec_indent();
    emit_line(")");
  }
}

static void scm_sr_emit_data_mem(Data* data) {
  emit_line("(define-syntax dmem-init!");
  inc_indent();
  emit_line("(syntax-rules (quote) ((_ s) (ck s '");
  inc_indent();
  scm_sr_emit_data_mem_rec(data, 0, 1 << 24);
  dec_indent();
  emit_line("))))");
  dec_indent();
}

static const char* SCM_SR_REG_NAMES[6] = {
  "\"A\"", "\"B\"", "\"C\"", "\"D\"", "\"BP\"", "\"SP\""
};

void target_scm_sr(Module* module) {
  reg_names = SCM_SR_REG_NAMES;

  scm_sr_emit_file_prologue();
  emit_line("");

  scm_sr_emit_func_impl(module->text);
  emit_line("");

  scm_sr_emit_inst_mem(scm_sc_count_pc(module->text));
  emit_line("");

  scm_sr_emit_data_mem(module->data);
  emit_line("");

  scm_sr_emit_file_epilogue();
}
