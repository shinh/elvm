#include <ir/ir.h>
#include <target/util.h>

static const char* j_cmp_str(Inst* inst) {
  int op = normalize_cond(inst->op, 0);
  const char* op_str;
  switch (op) {
    case JEQ:
      op_str = "="; break;
    case JNE:
      op_str = "~:"; break;
    case JLT:
      op_str = "<"; break;
    case JGT:
      op_str = ">"; break;
    case JLE:
      op_str = "<:"; break;
    case JGE:
      op_str = ">:"; break;
    case JMP:
      return "1";
    default:
      error("oops");
  }
  return format("%s %s %s", reg_names[inst->dst.reg], op_str, src_str(inst));
}

static void j_init_state(void) {
  for (int i = 0; i < 7; i++) {
    emit_line("%s=: 0", reg_names[i]);
  }
  emit_line("mem=: 16777216 $ 0");
  emit_line("input=: 1!:1@3 ''");
  emit_line("pos=: 0");
  emit_line("getc=: 3 : 0");
  emit_line("if. pos >: ($ input) do. 0 return. end.");
  emit_line("pos=: >:pos");
  emit_line("a. i. ((<:pos) { input)");
  emit_line(")");
}

static void j_emit_func_prologue(int func_id) {
  emit_line("");
  emit_line("func%d=: 3 : 0", func_id);
  emit_line("while. ((%d <: pc) *. (pc < %d)) do.",
            func_id * CHUNKED_FUNC_SIZE, (func_id + 1) * CHUNKED_FUNC_SIZE);
  inc_indent();
  emit_line("select. pc");
  emit_line("case. _1 do.");
  inc_indent();
}

static void j_emit_func_epilogue(void) {
  dec_indent();
  emit_line("end.");
  emit_line("pc=: >:pc");
  dec_indent();
  emit_line("end.");
  emit_line(")");
}

static void j_emit_pc_change(int pc) {
  dec_indent();
  emit_line("case. %d do.", pc);
  inc_indent();
}

static void j_emit_inst(Inst* inst) {
  switch (inst->op) {
  case MOV:
    emit_line("%s=: %s", reg_names[inst->dst.reg], src_str(inst));
    break;

  case ADD:
    emit_line("%s=: (%s + %s) (17 b.) " UINT_MAX_STR,
              reg_names[inst->dst.reg],
              reg_names[inst->dst.reg], src_str(inst));
    break;

  case SUB:
    emit_line("%s=: (%s - %s) (17 b.) " UINT_MAX_STR,
              reg_names[inst->dst.reg],
              reg_names[inst->dst.reg], src_str(inst));
    break;

  case LOAD:
    emit_line("%s=: %s { mem", reg_names[inst->dst.reg], src_str(inst));
    break;

  case STORE:
    emit_line("mem=: %s (%s)} mem", reg_names[inst->dst.reg], src_str(inst));
    break;

  case PUTC:
    emit_line("tmoutput (%s { a.)", src_str(inst));
    break;

  case GETC:
    emit_line("%s=: getc''", reg_names[inst->dst.reg]);
    break;

  case EXIT:
    emit_line("exit 0");
    break;

  case DUMP:
    break;

  case EQ:
  case NE:
  case LT:
  case GT:
  case LE:
  case GE:
    emit_line("%s=: %s", reg_names[inst->dst.reg], j_cmp_str(inst));
    break;

  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
  case JMP:
    emit_line("if. %s do. pc=: %s - 1 end.",
              j_cmp_str(inst), value_str(&inst->jmp));
    break;

  default:
    error("oops");
  }
}

void target_j(Module* module) {
  j_init_state();

  int num_funcs = emit_chunked_main_loop(module->text,
                                         j_emit_func_prologue,
                                         j_emit_func_epilogue,
                                         j_emit_pc_change,
                                         j_emit_inst);

  Data* data = module->data;
  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      emit_line("mem=: %d (%d)} mem", data->v, mp);
    }
  }

  emit_line("");
  emit_line("main=: 3 : 0");
  emit_line("while. 1 do.");
  inc_indent();
  emit_line("select. pc <.@%% %d", CHUNKED_FUNC_SIZE);
  for (int i = 0; i < num_funcs; i++) {
    emit_line("case. %d do. func%d''", i, i);
  }
  emit_line("end.");
  dec_indent();
  emit_line("end.");
  emit_line(")");
  emit_line("main''");
  emit_line("exit 1");
}
