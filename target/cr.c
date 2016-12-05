#include <ir/ir.h>
#include <target/util.h>

static const char* CR_REG_NAMES[] = {
  "REG[:a]", "REG[:b]", "REG[:c]", "REG[:d]", "REG[:bp]", "REG[:sp]", "REG[:pc]"
};

static void cr_init_state(Data* data) {
  emit_line("{%% if flag?(:cr_elvm_input) %%}");
  emit_line(" print \"#{STDIN.each_byte.to_a.map(&.to_i32).inspect} of Int32\"");
  emit_line("{%% elsif flag?(:cr_elvm_output) %%}");
  emit_line(" STDERR.write_byte ARGV[0].to_u8");
  emit_line("{%% else %%}");
  inc_indent();

  emit_line("STATE = {input: 0, exit: 0}");
  emit_line("");

  reg_names = CR_REG_NAMES;
  emit_line("REG = {a: 0, b: 0, c: 0, d: 0, bp: 0, sp: 0, pc: 0}");
  emit_line("");

  emit_line("MEM = {");
  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      emit_line(" %d => %d,", mp, data->v);
    }
  }
  emit_line("}");
  emit_line("");

  emit_line("INPUT = {{ `crystal run -D cr_elvm_input #{1.filename}` }}");
  dec_indent();
  emit_line("{%% end %%}");
}

static void cr_emit_func_prologue(int func_id) {
  emit_line("{%% if %d <= REG[:pc] && REG[:pc] < %d %%}",
            func_id * CHUNKED_FUNC_SIZE, (func_id + 1) * CHUNKED_FUNC_SIZE);
  inc_indent();
  emit_line("{%% loop2 = STATE[:exit] == 1 ? [] of Int32 : [0] %%}");
  emit_line("{%% for x in loop2 %%}");
  inc_indent();
  emit_line("{%% if STATE[:exit] == 0 && loop2.size < 100000 && (%d <= REG[:pc] && REG[:pc] < %d)",
            func_id * CHUNKED_FUNC_SIZE, (func_id + 1) * CHUNKED_FUNC_SIZE);
  inc_indent();
  emit_line("loop2.push 0");
  emit_line("if false");
  inc_indent();
}

static void cr_emit_func_epilogue(void) {
  dec_indent();
  emit_line("end");
  emit_line("REG[:pc] = REG[:pc] + 1");
  dec_indent();
  emit_line("end %%}");
  dec_indent();
  emit_line("{%% end %%}");
  dec_indent();
  emit_line("{%% end %%}");
}

static void cr_emit_pc_change(int pc) {
  dec_indent();
  emit_line("elsif REG[:pc] == %d", pc);
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
    emit_line("%s = MEM[%s] || 0", reg_names[inst->dst.reg], src_str(inst));
    break;

  case STORE:
    emit_line("MEM[%s] = %s", src_str(inst), reg_names[inst->dst.reg]);
    break;

  case PUTC:
    emit_line("`crystal run -D cr_elvm_output #{1.filename} -- #{%s & 255}`", src_str(inst));
    break;

  case GETC:
    emit_line("if c = INPUT[STATE[:input]]");
    emit_line(" %s = c", reg_names[inst->dst.reg]);
    emit_line(" STATE[:input] = STATE[:input] + 1");
    emit_line("else");
    emit_line(" %s = 0", reg_names[inst->dst.reg]);
    emit_line("end");
    break;

  case EXIT:
    emit_line("STATE[:exit] = 1");
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
    emit_line("REG[:pc] = %s ? %s - 1 : REG[:pc]",
              cmp_str(inst, "true"), value_str(&inst->jmp));
    break;

  case JMP:
    emit_line("REG[:pc] = %s - 1", value_str(&inst->jmp));
    break;

  default:
    error("oops");
  }
}

void target_cr(Module* module) {
  cr_init_state(module->data);
  emit_line("{%% unless flag?(:cr_elvm_input) || flag?(:cr_elvm_output) %%}");
  inc_indent();
  emit_line("{%% loop1 = [0] %%}");
  emit_line("{%% for x in loop1 %%}");
  inc_indent();
  emit_line("{%% loop2 = [] of Int32 %%}");
  emit_line("{%% unless STATE[:exit] == 1 %%}");
  emit_line(" {%% loop1.push 0 %%}");
  emit_line("{%% end %%}");
  emit_chunked_main_loop(module->text,
                         cr_emit_func_prologue,
                         cr_emit_func_epilogue,
                         cr_emit_pc_change,
                         cr_emit_inst);
  dec_indent();
  emit_line("{%% end %%}");
  dec_indent();
  emit_line("{%% end %%}");
}
