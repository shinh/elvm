#include <ir/ir.h>
#include <target/util.h>

static const char* CR_REG_NAMES[] = {
  "@a", "@b", "@c", "@d", "@bp", "@sp", "@pc"
};

static void init_state_cr(Data* data) {
  reg_names = CR_REG_NAMES;
  emit_line("module Main");
  inc_indent();
  for (int i = 0; i < 7; i++) {
    emit_line("property %s", reg_names[i] + 1);
  }
  emit_line("");
  emit_line("def initialize");
  inc_indent();
  for (int i = 0; i < 7; i++) {
    emit_line("%s = 0", reg_names[i]);
  }
  emit_line("@mem = [0] * (1 << 24)");
  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      emit_line("@mem[%d] = %d", mp, data->v);
    }
  }
  dec_indent();
  emit_line("end");
}

static void cr_emit_func_prologue(int func_id) {
  emit_line("");
  emit_line("def func%d", func_id);
  inc_indent();
  emit_line("while %d <= @pc && @pc < %d",
            func_id * CHUNKED_FUNC_SIZE, (func_id + 1) * CHUNKED_FUNC_SIZE);
  inc_indent();
  emit_line("case @pc");
  inc_indent();
}

static void cr_emit_func_epilogue(void) {
  dec_indent();
  emit_line("end");
  emit_line("@pc += 1");
  dec_indent();
  emit_line("end");
  dec_indent();
  emit_line("end");
}

static void cr_emit_pc_change(int pc) {
  emit_line("");
  dec_indent();
  emit_line("when %d", pc);
  inc_indent();
}

static void cr_emit_inst(Inst* inst) {
  switch (inst->op) {
  case MOV:
    emit_line("%s = %s", reg_names[inst->dst.reg], src_str(inst));
    break;

  case ADD:
    emit_line("%s = (%s + %s) & " UINT_MAX_STR,
              reg_names[inst->dst.reg],
              reg_names[inst->dst.reg], src_str(inst));
    break;

  case SUB:
    emit_line("%s = (%s - %s) & " UINT_MAX_STR,
              reg_names[inst->dst.reg],
              reg_names[inst->dst.reg], src_str(inst));
    break;

  case LOAD:
    emit_line("%s = @mem[%s]", reg_names[inst->dst.reg], src_str(inst));
    break;

  case STORE:
    emit_line("@mem[%s] = %s", src_str(inst), reg_names[inst->dst.reg]);
    break;

  case PUTC:
    emit_line("putc %s", src_str(inst));
    break;

  case GETC:
    emit_line("c = STDIN.getc; %s = c ? c.ord : 0",
              reg_names[inst->dst.reg]);
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
    emit_line("%s = %s ? 1 : 0",
              reg_names[inst->dst.reg], cmp_str(inst, "true"));
    break;

  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
  case JMP:
    emit_line("%s && @pc = %s - 1",
              cmp_str(inst, "true"), value_str(&inst->jmp));
    break;

  default:
    error("oops");
  }
}

void target_cr(Module* module) {
  init_state_cr(module->data);
  emit_line("");

  int num_funcs = emit_chunked_main_loop(module->text,
                                         cr_emit_func_prologue,
                                         cr_emit_func_epilogue,
                                         cr_emit_pc_change,
                                         cr_emit_inst);
  dec_indent();
  emit_line("end");

  emit_line("");
  emit_line("main = Main.new");
  emit_line("while true");
  inc_indent();
  emit_line("case main.pc / %d", CHUNKED_FUNC_SIZE);
  for (int i = 0; i < num_funcs; i++) {
    emit_line("when %d", i);
    emit_line(" main.func%d", i);
  }
  emit_line("end");
  dec_indent();
  emit_line("end");
}
