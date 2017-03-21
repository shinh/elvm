#include <ir/ir.h>
#include <target/util.h>

static const char* PL_REG_NAMES[] = {
  "$a", "$b", "$c", "$d", "$bp", "$sp", "$pc"
};

static void init_state_pl(Data* data) {
  emit_line("#!/usr/bin/env perl");
  emit_line("use 5.008;");
  emit_line("use strict;");
  emit_line("use warnings;");
  emit_line("use utf8;");
  emit_line("$| = 1;");
  emit_line("");
  reg_names = PL_REG_NAMES;
  for (int i = 0; i < 7; i++) {
    emit_line("my %s = 0;", reg_names[i]);
  }
  emit_line("my @mem = (");
  inc_indent();
  for (int mp = 0; data; data = data->next, mp++) {
    emit_line("%d,", data->v);
  }
  dec_indent();
  emit_line(");");
}

static void pl_emit_inst(Inst* inst) {
  switch (inst->op) {
  case MOV:
    emit_line("%s = %s;", reg_names[inst->dst.reg], src_str(inst));
    break;

  case ADD:
    emit_line("%s = (%s + %s) & " UINT_MAX_STR ";",
              reg_names[inst->dst.reg],
              reg_names[inst->dst.reg], src_str(inst));
    break;

  case SUB:
    emit_line("%s = (%s - %s) & " UINT_MAX_STR ";",
              reg_names[inst->dst.reg],
              reg_names[inst->dst.reg], src_str(inst));
    break;

  case LOAD:
    emit_line("%s = $mem[%s]||0;", reg_names[inst->dst.reg], src_str(inst));
    break;

  case STORE:
    emit_line("$mem[%s] = %s;", src_str(inst), reg_names[inst->dst.reg]);
    break;

  case PUTC:
    emit_line("print chr(%s);", src_str(inst));
    break;

  case GETC:
    emit_line("$c = getc(); %s = defined $c ? ord($c) : 0;",
              reg_names[inst->dst.reg]);
    break;

  case EXIT:
    emit_line("exit;");
    break;

  case DUMP:
    break;

  case EQ:
  case NE:
  case LT:
  case GT:
  case LE:
  case GE:
    emit_line("%s = %s ? 1 : 0;",
              reg_names[inst->dst.reg], cmp_str(inst, "1"));
    break;

  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
    emit_line("$pc = %s - 1 if %s;",
              value_str(&inst->jmp), cmp_str(inst, "1"));
    break;
  case JMP:
    emit_line("$pc = %s - 1;",
              value_str(&inst->jmp));
    break;

  default:
    error("oops");
  }
}

void target_pl(Module* module) {
  init_state_pl(module->data);
  emit_line("");

  emit_line("");
  emit_line("my @codes; @codes = (");
  inc_indent();

  int prev_pc = -1;
  for (Inst* inst = module->text; inst; inst = inst->next) {
    if (prev_pc != inst->pc) {
      if (prev_pc != -1) {
        emit_line("goto $codes[++$pc];");
        dec_indent();
        emit_line("},");
      }
      emit_line("sub {");
      inc_indent();
      prev_pc = inst->pc;
    }
    pl_emit_inst(inst);
  }

  dec_indent();
  emit_line("});");
  emit_line("$codes[0]->();");
}
