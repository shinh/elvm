#include <ir/ir.h>
#include <target/util.h>

static const char* PHP_REG_NAMES[] = {
  "$a", "$b", "$c", "$d", "$bp", "$sp", "$pc"
};

static void init_state_php(Data* data) {
  reg_names = PHP_REG_NAMES;
  emit_line("<?php");

  emit_line("call_user_func(function(){");
  inc_indent();
  for (int i = 0; i < 7; i++) {
    emit_line("%s = 0;", reg_names[i]);
  }
  emit_line("$running = true;");
  emit_line("$mem = array();");
  /* Array initialization wastes a lot of memory */
  emit_line("ini_set(\"memory_limit\", \"768M\");");
  emit_line("// for ($_ = 0; $_ < (1 << 24); $_++) $mem[$_] = null; unset($_);");

  emit_line("$stdin = fopen('php://stdin', 'r');");
  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      emit_line("$mem[%d] = %d;", mp, data->v);
    }
  }
  emit_line("goto main;");
}

static void php_emit_func_prologue(int func_id) {
  emit_line("");
  emit_line("proc%d:", func_id);
  inc_indent();
  emit_line("while (%d <= $pc && $pc < %d && $running) {",
            func_id * CHUNKED_FUNC_SIZE, (func_id + 1) * CHUNKED_FUNC_SIZE);
  inc_indent();
  emit_line("switch ($pc) {");
  emit_line("case -1:  // dummy");
  inc_indent();
}

static void php_emit_func_epilogue(void) {
  dec_indent();
  emit_line("}");
  emit_line("$pc++;");
  dec_indent();
  emit_line("}");
  dec_indent();
  emit_line("goto main;");
}

static void php_emit_pc_change(int pc) {
  emit_line("break;");
  emit_line("");
  dec_indent();
  emit_line("case %d:", pc);
  inc_indent();
}

static void php_emit_inst(Inst* inst) {
  switch (inst->op) {
  case MOV:
    emit_line("%s = %s;", reg_names[inst->dst.reg], src_str(inst));
    break;

  case ADD:
    emit_line("%s = (%s + %s) & " UINT_MAX_STR ";",
              reg_names[inst->dst.reg],
              reg_names[inst->dst.reg],
              src_str(inst));
    break;

  case SUB:
    emit_line("%s = (%s - %s) & " UINT_MAX_STR ";",
              reg_names[inst->dst.reg],
              reg_names[inst->dst.reg],
              src_str(inst));
    break;

  case LOAD:
    emit_line("%s = @$mem[%s] ?: 0; // @undefined index as 0",
              reg_names[inst->dst.reg], src_str(inst));
    break;

  case STORE:
    emit_line("$mem[%s] = %s;", src_str(inst), reg_names[inst->dst.reg]);
    break;

  case PUTC:
    emit_line("printf(\"%%c\", %s);", src_str(inst));
    break;

  case GETC:
    emit_line("%s = ord(fgetc($stdin));",
              reg_names[inst->dst.reg]);
    break;

  case EXIT:
    emit_line("$running = false; break;");
    break;

  case DUMP:
    break;

  case EQ:
  case NE:
  case LT:
  case GT:
  case LE:
  case GE:
    emit_line("%s = (%s) | 0;",
              reg_names[inst->dst.reg], cmp_str(inst, "true"));
    break;

  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
  case JMP:
    emit_line("if (%s) $pc = %s - 1;",
              cmp_str(inst, "true"), value_str(&inst->jmp));
    break;

  default:
    error("oops");
  }
}

void target_php(Module* module) {
  init_state_php(module->data);

  int num_funcs = emit_chunked_main_loop(module->text,
                                         php_emit_func_prologue,
                                         php_emit_func_epilogue,
                                         php_emit_pc_change,
                                         php_emit_inst);

  emit_line("");
  emit_line("main:");
  emit_line("while ($running) {");
  inc_indent();
  emit_line("switch ($pc / %d | 0) {", CHUNKED_FUNC_SIZE);
  for (int i = 0; i < num_funcs; i++) {
    emit_line("case %d:", i);
    emit_line(" goto proc%d;", i);
    emit_line(" break;");
  }
  emit_line("}");
  dec_indent();
  emit_line("}");
  dec_indent();
  emit_line("});");
}
