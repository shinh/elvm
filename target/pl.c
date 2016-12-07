#include <ir/ir.h>
#include <target/util.h>

static const char* PL_REG_NAMES[] = {
  "$a", "$b", "$c", "$d", "$bp", "$sp", "$pc"
};

static void init_state_pl(Data* data) {
  emit_line("#!/usr/bin/env perl");
  emit_line("use strict;");
  emit_line("use warnings;");
  emit_line("use utf8;");
  emit_line("$| = 1;");
  emit_line("");
  reg_names = PL_REG_NAMES;
  for (int i = 0; i < 7; i++) {
    emit_line("my %s = 0;", reg_names[i]);
  }
  emit_line("my @mem = ();");
  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      emit_line("$mem[%d] = %d;", mp, data->v);
    }
  }
}

static void pl_emit_func_prologue(int func_id) {
  emit_line("");
  emit_line("sub func%d {", func_id);
  inc_indent();
  emit_line("while (%d <= $pc && $pc < %d) {",
            func_id * CHUNKED_FUNC_SIZE, (func_id + 1) * CHUNKED_FUNC_SIZE);
  inc_indent();
  emit_line("if (0) {");
  inc_indent();
}

static void pl_emit_func_epilogue(void) {
  dec_indent();
  emit_line("}");
  emit_line("$pc++;");
  dec_indent();
  emit_line("}");
  dec_indent();
  emit_line("}");
}

static void pl_emit_pc_change(int pc) {
  dec_indent();
  emit_line("}");
  emit_line("elsif ($pc ==  %d) {", pc);
  inc_indent();
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
    emit_line("%s = ($mem[%s] //= 0);", reg_names[inst->dst.reg], src_str(inst));
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
    emit_line("%s = (defined %s && %s) ? 1 : 0;",
              reg_names[inst->dst.reg], reg_names[inst->dst.reg], cmp_str(inst, "1"));
    break;

  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
    emit_line("$pc = %s - 1 if defined %s && %s;",
              value_str(&inst->jmp), reg_names[inst->dst.reg], cmp_str(inst, "1"));
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

  int num_funcs = emit_chunked_main_loop(module->text,
                                         pl_emit_func_prologue,
                                         pl_emit_func_epilogue,
                                         pl_emit_pc_change,
                                         pl_emit_inst);

  emit_line("");
  emit_line("while (1) {");
  inc_indent();
  emit_line("my $e = int($pc / %d);", CHUNKED_FUNC_SIZE);
  emit_line("if (0) {", CHUNKED_FUNC_SIZE);
  emit_line("}", CHUNKED_FUNC_SIZE);
  for (int i = 0; i < num_funcs; i++) {
    emit_line("elsif ($e == %d) {", i);
    inc_indent();
    emit_line("func%d;", i);
    dec_indent();
    emit_line("}");
  }
  dec_indent();
  emit_line("}");
}
