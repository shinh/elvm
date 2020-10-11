#include <ir/ir.h>
#include <target/util.h>

static const char* tcl_value_str(Value* v) {
  if (v->type == REG) {
    return format("$%s", reg_names[v->reg]);
  } else if (v->type == IMM) {
    return format("%d", v->imm);
  } else {
    error("invalid value");
  }
}

static const char* tcl_src_str(Inst* inst) {
  return tcl_value_str(&inst->src);
}

static const char* tcl_cmp_str(Inst* inst) {
  int op = normalize_cond(inst->op, 0);
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
      return "1";
    default:
      error("oops");
  }
  return format("$%s %s %s", reg_names[inst->dst.reg], op_str, tcl_src_str(inst));
}

static void init_state_tcl(Data* data) {
  for (int i = 0; i < 7; i++) {
    emit_line("set %s 0", reg_names[i]);
  }
  emit_line("set mem [lrepeat [expr {1 << 24}] 0]");
  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      emit_line("lset mem %d %d", mp, data->v);
    }
  }
  emit_line("");
  emit_line("proc getc {} {");
  inc_indent();
  emit_line("set ch [read stdin 1]");
  emit_line("return [expr {$ch == \"\" ? 0 : [scan $ch \"%%c\"]}]");
  dec_indent();
  emit_line("}");
}

static void tcl_emit_func_prologue(int func_id) {
  emit_line("");
  emit_line("proc func%d {} {", func_id);
  inc_indent();
  for (int i = 0; i < 7; i++) {
    emit_line("global %s", reg_names[i]);
  }
  emit_line("global mem");
  emit_line("");

  emit_line("while {%d <= $pc && $pc < %d} {",
            func_id * CHUNKED_FUNC_SIZE, (func_id + 1) * CHUNKED_FUNC_SIZE);
  inc_indent();
  emit_line("switch $pc {");
  inc_indent();
  emit_line("-1 {");
  inc_indent();
}

static void tcl_emit_func_epilogue(void) {
  dec_indent();
  emit_line("}");
  dec_indent();
  emit_line("}");
  emit_line("incr pc");
  dec_indent();
  emit_line("}");
  dec_indent();
  emit_line("}");
}

static void tcl_emit_pc_change(int pc) {
  dec_indent();
  emit_line("}");
  emit_line("%d {", pc);
  inc_indent();
}

static void tcl_emit_inst(Inst* inst) {
  switch (inst->op) {
  case MOV:
    emit_line("set %s %s", reg_names[inst->dst.reg], tcl_src_str(inst));
    break;

  case ADD:
    emit_line("set %s [expr {($%s + %s) & " UINT_MAX_STR "}]",
              reg_names[inst->dst.reg],
              reg_names[inst->dst.reg], tcl_src_str(inst));
    break;

  case SUB:
    emit_line("set %s [expr {($%s - %s) & " UINT_MAX_STR "}]",
              reg_names[inst->dst.reg],
              reg_names[inst->dst.reg], tcl_src_str(inst));
    break;

  case LOAD:
    emit_line("set %s [lindex $mem %s]",
              reg_names[inst->dst.reg], tcl_src_str(inst));
    break;

  case STORE:
    emit_line("lset mem %s $%s", tcl_src_str(inst), reg_names[inst->dst.reg]);
    break;

  case PUTC:
    emit_line("puts -nonewline [format \"%%c\" %s]", tcl_src_str(inst));
    break;

  case GETC:
    emit_line("set %s [getc]", reg_names[inst->dst.reg]);
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
    emit_line("set %s [expr {%s}]", reg_names[inst->dst.reg], tcl_cmp_str(inst));
    break;

  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
  case JMP:
    emit_line("if {%s} { set pc [expr {%s - 1}] }",
              tcl_cmp_str(inst), tcl_value_str(&inst->jmp));
    break;

  default:
    error("oops");
  }
}

void target_tcl(Module* module) {
  init_state_tcl(module->data);

  int num_funcs = emit_chunked_main_loop(module->text,
                                         tcl_emit_func_prologue,
                                         tcl_emit_func_epilogue,
                                         tcl_emit_pc_change,
                                         tcl_emit_inst);

  emit_line("");
  emit_line("while 1 {");
  inc_indent();
  emit_line("switch [expr {$pc / %d}] {", CHUNKED_FUNC_SIZE);
  inc_indent();
  for (int i = 0; i < num_funcs; i++) {
    emit_line("%d { func%d }", i, i);
  }
  dec_indent();
  emit_line("}");
  dec_indent();
  emit_line("}");
}
