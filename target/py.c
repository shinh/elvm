#include <ir/ir.h>
#include <target/util.h>

static void init_state_py(Data* data) {
  emit_line("import sys");
  for (int i = 0; i < 7; i++) {
    emit_line("%s = 0", reg_names[i]);
  }
  emit_line("mem = [0] * (1 << 24)");
  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      emit_line("mem[%d] = %d", mp, data->v);
    }
  }
}

static void py_emit_func_prologue(int func_id) {
  emit_line("");
  emit_line("def func%d():", func_id);
  inc_indent();
  for (int i = 0; i < 7; i++) {
    emit_line("global %s", reg_names[i]);
  }
  emit_line("global mem");
  emit_line("");

  emit_line("while %d <= pc and pc < %d:",
            func_id * CHUNKED_FUNC_SIZE, (func_id + 1) * CHUNKED_FUNC_SIZE);
  inc_indent();
  emit_line("if False:");
  inc_indent();
  emit_line("pass");
}

static void py_emit_func_epilogue(void) {
  dec_indent();
  emit_line("pc += 1");
  dec_indent();
  dec_indent();
}

static void py_emit_pc_change(int pc) {
  emit_line("");
  dec_indent();
  emit_line("elif pc == %d:", pc);
  inc_indent();
}

static void py_emit_inst(Inst* inst) {
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
    emit_line("%s = mem[%s]", reg_names[inst->dst.reg], src_str(inst));
    break;

  case STORE:
    emit_line("mem[%s] = %s", src_str(inst), reg_names[inst->dst.reg]);
    break;

  case PUTC:
    emit_line("sys.stdout.write(chr(%s))", src_str(inst));
    break;

  case GETC:
    emit_line("_ = sys.stdin.read(1); %s = ord(_) if _ else 0",
              reg_names[inst->dst.reg]);
    break;

  case EXIT:
    emit_line("sys.exit(0)");
    break;

  case DUMP:
    break;

  case EQ:
  case NE:
  case LT:
  case GT:
  case LE:
  case GE:
    emit_line("%s = int(%s)",
              reg_names[inst->dst.reg], cmp_str(inst, "True"));
    break;

  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
  case JMP:
    emit_line("if %s: pc = %s - 1",
              cmp_str(inst, "True"), value_str(&inst->jmp));
    break;

  default:
    error("oops");
  }
}

void target_py(Module* module) {
  init_state_py(module->data);

  int num_funcs = emit_chunked_main_loop(module->text,
                                         py_emit_func_prologue,
                                         py_emit_func_epilogue,
                                         py_emit_pc_change,
                                         py_emit_inst);

  emit_line("");
  emit_line("while True:");
  inc_indent();
  emit_line("if False: pass");
  for (int i = 0; i < num_funcs; i++) {
    emit_line("elif pc < %d: func%d()", (i + 1) * CHUNKED_FUNC_SIZE, i);
  }
  dec_indent();
}
