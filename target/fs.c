#include <ir/ir.h>
#include <target/util.h>

static const char* FS_REG_NAMES[] = {
  "a", "b", "c", "d", "bp", "sp", "pc"
};

static void init_state_fs(Data* data) {
  reg_names = FS_REG_NAMES;
  for (int i = 0; i < 7; i++) {
    emit_line("let mutable %s = 0", reg_names[i]);
  }
  emit_line("let mem : int array = Array.zeroCreate (1 <<< 24)");
  emit_line("");
  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      emit_line("Array.set mem %d %d", mp, data->v);
    }
  }
}

static void fs_emit_func_prologue(int func_id) {
  emit_line("");
  emit_line("let func%d() =", func_id);
  inc_indent();
  emit_line("while %d <= pc && pc < %d do",
            func_id * CHUNKED_FUNC_SIZE, (func_id + 1) * CHUNKED_FUNC_SIZE);
  inc_indent();
  emit_line("match pc with");
  inc_indent();
}

static void fs_emit_func_epilogue(void) {
  dec_indent();
  emit_line("| _ -> failwith(\"oops\")");
  emit_line("pc <- pc + 1");
  dec_indent();
  dec_indent();
}

static void fs_emit_pc_change(int pc) {
  dec_indent();
  emit_line("| %d ->", pc);
  inc_indent();
}

static const char* fs_cmp_str(Inst* inst, const char* true_str) {
  int op = normalize_cond(inst->op, 0);
  const char* op_str;
  switch (op) {
    case JEQ:
      op_str = "="; break;
    case JNE:
      op_str = "<>"; break;
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

static void fs_emit_inst(Inst* inst) {
  switch (inst->op) {
  case MOV:
    emit_line("%s <- %s", reg_names[inst->dst.reg], src_str(inst));
    break;

  case ADD:
    emit_line("%s <- (%s + %s) &&& " UINT_MAX_STR,
              reg_names[inst->dst.reg],
              reg_names[inst->dst.reg], src_str(inst));
    break;

  case SUB:
    emit_line("%s <- (%s - %s) &&& " UINT_MAX_STR,
              reg_names[inst->dst.reg],
              reg_names[inst->dst.reg], src_str(inst));
    break;

  case LOAD:
    emit_line("%s <- mem.[%s]", reg_names[inst->dst.reg], src_str(inst));
    break;

  case STORE:
    emit_line("Array.set mem %s %s", src_str(inst), reg_names[inst->dst.reg]);
    break;

  case PUTC:
    emit_line("printf \"%%c\" (char %s)", src_str(inst));
    break;

  case GETC:
    emit_line("%s <- let x = Console.Read() in if x <> -1 then x else 0",
              reg_names[inst->dst.reg]);
    break;

  case EXIT:
    emit_line("Environment.Exit(0)");
    break;

  case DUMP:
    break;

  case EQ:
  case NE:
  case LT:
  case GT:
  case LE:
  case GE:
    emit_line("%s <- if %s then 1 else 0",
              reg_names[inst->dst.reg], fs_cmp_str(inst, "true"));
    break;

  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
  case JMP:
    emit_line("if %s then pc <- %s - 1",
              fs_cmp_str(inst, "true"), value_str(&inst->jmp));
    break;

  default:
    error("oops");
  }
}

void target_fs(Module* module) {
  emit_line("open System");
  emit_line("");

  init_state_fs(module->data);

  int num_funcs = emit_chunked_main_loop(module->text,
                                         fs_emit_func_prologue,
                                         fs_emit_func_epilogue,
                                         fs_emit_pc_change,
                                         fs_emit_inst);

  emit_line("");
  emit_line("[<EntryPoint>]");
  emit_line("let main argv =");
  inc_indent();

  emit_line("while true do");
  inc_indent();
  emit_line("match pc / %d with", CHUNKED_FUNC_SIZE);
  for (int i = 0; i < num_funcs; i++) {
    emit_line("| %d -> func%d()", i, i);
  }
  emit_line("| _ -> failwith(\"oops\")");
  dec_indent();
  emit_line("0");
}
