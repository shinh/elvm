#include <ir/ir.h>
#include <target/util.h>

static const char* OCT_REG_NAMES[] = {
  "A", "B", "C", "D", "BP", "SP", "PC"
};

static void init_state_oct(Data* data) {
  reg_names = OCT_REG_NAMES;
  for (int i = 0; i < 7; i++) {
    emit_line("global %s = 0", reg_names[i]);
  }
  emit_line("global mem = zeros(1, 2 ** 24)");
  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      emit_line("mem(%d + 1) = %d;", mp, data->v);
    }
  }
}

static void oct_emit_func_prologue(int func_id) {
  emit_line("");
  emit_line("function ret = func%d", func_id);
  inc_indent();
  for (int i = 0; i < 7; i++) {
    emit_line("global %s", reg_names[i]);
  }
  emit_line("global mem");
  emit_line("ret = true;");
  emit_line("while %d <= PC && PC < %d",
            func_id * CHUNKED_FUNC_SIZE, (func_id + 1) * CHUNKED_FUNC_SIZE);
  inc_indent();
  emit_line("switch PC");
  inc_indent();
}

static void oct_emit_func_epilogue(void) {
  dec_indent();
  emit_line("endswitch");
  emit_line("++PC;");
  dec_indent();
  emit_line("endwhile");
  dec_indent();
  emit_line("endfunction");
}

static void oct_emit_pc_change(int pc) {
  emit_line("");
  dec_indent();
  emit_line("case %d", pc);
  inc_indent();
}

static void oct_emit_inst(Inst* inst) {
  switch (inst->op) {
  case MOV:
    emit_line("%s = %s;", reg_names[inst->dst.reg], src_str(inst));
    break;

  case ADD:
    emit_line("%s = bitand(%s + %s, " UINT_MAX_STR ");",
              reg_names[inst->dst.reg],
              reg_names[inst->dst.reg], src_str(inst));
    break;

  case SUB:
    emit_line("d = %s - %s; %s = ifelse(d >= 0, bitand(d, " UINT_MAX_STR \
              "), bitand(d + 2 ** 24, " UINT_MAX_STR "));",
              reg_names[inst->dst.reg], src_str(inst),
              reg_names[inst->dst.reg]);
    break;

  case LOAD:
    emit_line("%s = mem(%s + 1);", reg_names[inst->dst.reg], src_str(inst));
    break;

  case STORE:
    emit_line("mem(%s + 1) = %s;", src_str(inst), reg_names[inst->dst.reg]);
    break;

  case PUTC:
    emit_line("puts(char(%s));", src_str(inst));
    break;

  case GETC:
    emit_line("c = fread(stdin, 1); %s = ifelse(isempty(c), 0, c);",
              reg_names[inst->dst.reg]);
    break;

  case EXIT:
    // octave's "exit" prints new line...
    emit_line("ret = false;");
    emit_line("return");
    break;

  case DUMP:
    break;

  case EQ:
  case NE:
  case LT:
  case GT:
  case LE:
  case GE:
    emit_line("%s = ifelse(%s, 1, 0);",
              reg_names[inst->dst.reg], cmp_str(inst, "true"));
    break;

  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
  case JMP:
    emit_line("(%s) && (PC = %s - 1);",
              cmp_str(inst, "true"), value_str(&inst->jmp));
    break;

  default:
    error("oops");
  }
}

void target_oct(Module* module) {
  init_state_oct(module->data);
  emit_line("");

  int num_funcs = emit_chunked_main_loop(module->text,
                                         oct_emit_func_prologue,
                                         oct_emit_func_epilogue,
                                         oct_emit_pc_change,
                                         oct_emit_inst);

  emit_line("");
  emit_line("while true");
  inc_indent();
  emit_line("switch floor(PC / %d)", CHUNKED_FUNC_SIZE);
  for (int i = 0; i < num_funcs; i++) {
    emit_line("case %d", i);
    inc_indent();
    emit_line("if !func%d", i);
    inc_indent();
    emit_line("break");
    dec_indent();
    emit_line("endif");
    dec_indent();
  }
  emit_line("endswitch");
  dec_indent();
  emit_line("endwhile");
}
