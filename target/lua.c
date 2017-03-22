#include <ir/ir.h>
#include <target/util.h>

const char* lua_cmp_str(Inst* inst, const char* true_str) {
  int op = normalize_cond(inst->op, 0);
  const char* op_str;
  switch (op) {
    case JEQ:
      op_str = "=="; break;
    case JNE:
      op_str = "~="; break;
    case JLT:
      op_str = "<"; break;
    case JGT:
      op_str = ">"; break;
    case JLE:
      op_str = "<="; break;
    case JGE:
      op_str = ">="; break;
    case JMP:
      return true_str;
    default:
      error("oops");
  }
  return format("%s %s %s", reg_names[inst->dst.reg], op_str, src_str(inst));
}

static void init_state_lua(Data* data) {
  for (int i = 0; i < 7; i++) {
    emit_line("%s = 0", reg_names[i]);
  }
  emit_line("mem = {}");
  emit_line("for _ = 0, ((1 << 24) -1) do mem[_] = 0; end");
  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      emit_line("mem[%d] = %d", mp, data->v);
    }
  }
}

static void lua_emit_func_prologue(int func_id) {
  emit_line("");
  emit_line("function func%d()", func_id);
  inc_indent();
  emit_line("");

  emit_line("while %d <= pc and pc < %d do",
            func_id * CHUNKED_FUNC_SIZE, (func_id + 1) * CHUNKED_FUNC_SIZE);
  inc_indent();
  emit_line("if false then");
  inc_indent();
}

static void lua_emit_func_epilogue(void) {
  dec_indent();
  emit_line("end");
  emit_line("pc = pc + 1");
  dec_indent();
  emit_line("end");
  dec_indent();
  emit_line("end");
}

static void lua_emit_pc_change(int pc) {
  emit_line("");
  dec_indent();
  emit_line("elseif pc == %d then", pc);
  inc_indent();
}

static void lua_emit_inst(Inst* inst) {
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
    emit_line("io.write(string.char(%s))", src_str(inst));
    break;

  case GETC:
    emit_line("_ = io.read(1); %s = _ and string.byte(_) or 0",
              reg_names[inst->dst.reg]);
    break;

  case EXIT:
    emit_line("os.exit(0)");
    break;

  case DUMP:
    break;

  case EQ:
  case NE:
  case LT:
  case GT:
  case LE:
  case GE:
    emit_line("%s = ((%s) and 1 or 0)",
              reg_names[inst->dst.reg], lua_cmp_str(inst, "true"));
    break;

  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
  case JMP:
    emit_line("if %s then pc = %s - 1; end",
              lua_cmp_str(inst, "true"), value_str(&inst->jmp));
    break;

  default:
    error("oops");
  }
}

void target_lua(Module* module) {
  init_state_lua(module->data);

  int num_funcs = emit_chunked_main_loop(module->text,
                                         lua_emit_func_prologue,
                                         lua_emit_func_epilogue,
                                         lua_emit_pc_change,
                                         lua_emit_inst);

  emit_line("");
  emit_line("while true do");
  inc_indent();
  emit_line("if false then");
  for (int i = 0; i < num_funcs; i++) {
    emit_line("elseif pc < %d then func%d();", (i + 1) * CHUNKED_FUNC_SIZE, i);
  }
  emit_line("end");
  dec_indent();
  emit_line("end");
}
