#include <ir/ir.h>
#include <target/util.h>

static void init_state_asmjs(Data* data) {
  emit_line("var main = function() {");
  emit_line("var mod = function(stdlib, foreign, heap) {");
  emit_line("\"use asm\";");
  emit_line("var mem = new stdlib.Int32Array(heap);");
  emit_line("var putchar = foreign.putchar;");
  emit_line("var getchar = foreign.getchar;");
  emit_line("var running = 1;");

  for (int i = 0; i < 7; i++) {
    emit_line("var %s = 0;", reg_names[i]);
  }
  emit_line("function init() {");
  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      emit_line("mem[%d] = %d;", mp, data->v);
    }
  }
  emit_line("}");
}

static void asmjs_emit_func_prologue(int func_id) {
  emit_line("");
  emit_line("function func%d() {", func_id);
  inc_indent();
  emit_line("while ((%d <= (pc | 0)) & ((pc | 0) < %d) & running) {",
            func_id * CHUNKED_FUNC_SIZE, (func_id + 1) * CHUNKED_FUNC_SIZE);
  inc_indent();
  emit_line("switch (pc | 0) {");
  emit_line("case -1:  // dummy");
  inc_indent();
}

static const char *unsigned_cmp_str(Inst *inst) {
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
  return format("(%s >>> 0) %s (%s >>> 0)", reg_names[inst->dst.reg], op_str, src_str(inst));
}

static void asmjs_emit_func_epilogue(void) {
  dec_indent();
  emit_line("}"); /* switch (pc) */
  emit_line("pc = (pc + 1) | 0;");
  dec_indent();
  emit_line("}"); /* while (_ <= pc && pc < _ && running) */
  dec_indent();
  emit_line("}"); /* function func%d */
}

static void asmjs_emit_pc_change(int pc) {
  emit_line("break;");
  emit_line("");
  dec_indent();
  emit_line("case %d:", pc);
  inc_indent();
}

static void asmjs_emit_inst(Inst* inst) {
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
    emit_line("%s = mem[(%s << 2) >> 2] | 0;", reg_names[inst->dst.reg], src_str(inst));
    break;

  case STORE:
    emit_line("mem[(%s << 2) >> 2] = %s;", src_str(inst), reg_names[inst->dst.reg]);
    break;

  case PUTC:
    emit_line("putchar(%s | 0);", src_str(inst));
    break;

  case GETC:
    emit_line("%s = getchar() | 0;",
              reg_names[inst->dst.reg]);
    break;

  case EXIT:
    emit_line("running = 0; break;");
    break;

  case DUMP:
    break;

  case EQ:
  case NE:
  case LT:
  case GT:
  case LE:
  case GE:
    emit_line("%s = %s;",
              reg_names[inst->dst.reg], unsigned_cmp_str(inst));
    break;

  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
  case JMP:
    emit_line("if (%s) pc = (%s - 1) | 0;",
              unsigned_cmp_str(inst), value_str(&inst->jmp));
    break;

  default:
    error("oops");
  }
}

void target_asmjs(Module* module) {
  init_state_asmjs(module->data);

  int num_funcs = emit_chunked_main_loop(module->text,
                                         asmjs_emit_func_prologue,
                                         asmjs_emit_func_epilogue,
                                         asmjs_emit_pc_change,
                                         asmjs_emit_inst);

  emit_line("");
  emit_line("function main() {");
  emit_line("init();");
  emit_line("while (running) {");
  inc_indent();
  emit_line("switch ((pc | 0) / %d | 0) {", CHUNKED_FUNC_SIZE);
  for (int i = 0; i < num_funcs; i++) {
    emit_line("case %d:", i);
    emit_line(" func%d();", i);
    emit_line(" break;");
  }
  emit_line("}"); /* switch (pc / CHUNKED_FUNC_SIZE) */
  dec_indent();
  emit_line("}"); /* while (running) */
  emit_line("}"); /* function main */

  emit_line("return main;");
  emit_line("};"); /* var mod = function() */
  emit_line("return function(getchar, putchar) {");
  emit_line("return mod((0,eval)('this'), {getchar: getchar, putchar: putchar}, new ArrayBuffer(1 << 26))();");
  emit_line("};"); /* function(getchar, putchar) */
  emit_line("}();"); /* var main = function() */

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
