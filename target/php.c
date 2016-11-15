#include <ir/ir.h>
#include <target/util.h>

static void init_state_js(Data* data) {
  emit_line("var main = function(getchar, putchar) {");

  for (int i = 0; i < 7; i++) {
    emit_line("var %s = 0;", reg_names[i]);
  }
  emit_line("var mem = new Int32Array(1 << 24);");
  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      emit_line("mem[%d] = %d;", mp, data->v);
    }
  }
}

static void js_emit_func_prologue(int func_id) {
  emit_line("");
  emit_line("var func%d = function() {", func_id);
  inc_indent();
  emit_line("while (%d <= pc && pc < %d && running) {",
            func_id * CHUNKED_FUNC_SIZE, (func_id + 1) * CHUNKED_FUNC_SIZE);
  inc_indent();
  emit_line("switch (pc) {");
  emit_line("case -1:  // dummy");
  inc_indent();
}

static void js_emit_func_epilogue(void) {
  dec_indent();
  emit_line("}");
  emit_line("pc++;");
  dec_indent();
  emit_line("}");
  dec_indent();
  emit_line("};");
}

static void js_emit_pc_change(int pc) {
  emit_line("break;");
  emit_line("");
  dec_indent();
  emit_line("case %d:", pc);
  inc_indent();
}

static void js_emit_inst(Inst* inst) {
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
    emit_line("%s = mem[%s];", reg_names[inst->dst.reg], src_str(inst));
    break;

  case STORE:
    emit_line("mem[%s] = %s;", src_str(inst), reg_names[inst->dst.reg]);
    break;

  case PUTC:
    emit_line("putchar(%s);", src_str(inst));
    break;

  case GETC:
    emit_line("%s = getchar();",
              reg_names[inst->dst.reg]);
    break;

  case EXIT:
    emit_line("running = false; break;");
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
    emit_line("if (%s) pc = %s - 1;",
              cmp_str(inst, "true"), value_str(&inst->jmp));
    break;

  default:
    error("oops");
  }
}

void target_js(Module* module) {
  init_state_js(module->data);

  emit_line("var running = true;");

  int num_funcs = emit_chunked_main_loop(module->text,
                                         js_emit_func_prologue,
                                         js_emit_func_epilogue,
                                         js_emit_pc_change,
                                         js_emit_inst);

  emit_line("");
  emit_line("while (running) {");
  inc_indent();
  emit_line("switch (pc / %d | 0) {", CHUNKED_FUNC_SIZE);
  for (int i = 0; i < num_funcs; i++) {
    emit_line("case %d:", i);
    emit_line(" func%d();", i);
    emit_line(" break;");
  }
  emit_line("}");
  dec_indent();
  emit_line("}");

  emit_line("};");

  // For nodejs
  emit_line("if (typeof require != 'undefined') {");
  emit_line(" var sys = require('sys');");
  emit_line(" var input = null;");
  emit_line(" var ip = 0;");
  emit_line(" var getchar = function() {");
  emit_line("  if (input === null)");
  emit_line("   input = require('fs').readFileSync('/dev/stdin');");
  emit_line("  return input[ip++] | 0;");
  emit_line(" };");
  emit_line(" var putchar = function(c) {");
  emit_line("  sys.print(String.fromCharCode(c & 255));");
  emit_line(" };");
  emit_line(" main(getchar, putchar);");
  emit_line("}");
}
