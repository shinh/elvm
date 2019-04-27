#include <ir/ir.h>
#include <target/util.h>
#include <string.h>

static void init_state_lol(Data* data) {
  emit_line("HAI 1.4");
  emit_line("CAN HAS STDLIB?");
  for (int i = 0; i < 7; i++) {
    emit_line("I HAS A %s ITZ 0", reg_names[i]);
  }
  emit_line("I HAS A mem ITZ A BUKKIT");
  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      emit_line("mem HAS A m%d ITZ %d", mp, data->v);
    }
  }
}

static void lol_cmp_str(int op, char* left, char* right){
  char* output;
  switch (op){
    case JEQ:
    case EQ:
      sprintf(output, "BOTH SAEM %s AN %s", left, right);
      break;
    case JNE:
    case NE:
      sprintf(output, "DIFFRINT %s AN %s", left, right);
      break;
    case JLE:
    case LE:
      sprintf(output, "BOTH SAEM %s AN SMALLR OF %s AN %s", left, left, right);
      break;
    case JGE:
    case GE:
      sprintf(output, "BOTH SAEM %s AN BIGGR OF %s AN %s", left, left, right);
      break;
    case JLT:
    case LT:
      sprintf(output, "DIFFRINT %s AN BIGGR OF %s AN %s", left, left, right);
      break;
    case JGT
    case GT:
      sprintf(output, "DIFFRINT %s AN SMALLR OF %s AN %s", left, left, right);
      break;
    case JMP:
      output = "WIN";
      break;
  }
  return output;
}

static void lol_emit_func_prologue(int func_id) {
  emit_line("");
  emit_line("HOW IZ I func%d", func_id);
  inc_indent();
//   for (int i = 0; i < 7; i++) {
//     emit_line("global %s", reg_names[i]);
//   }
//   emit_line("global mem");
//   emit_line("");
  emit_line("I HAZ A ACC ITZ 0");
  emit_line("IM IN YR LOOP UPPIN YR ACC WILE BOTH OF BOTH SAEM %d AN SMALLR OF %d AN pc AN DIFFRINT pc AN BIGGR OF %d AN pc",
            func_id * CHUNKED_FUNC_SIZE, func_id * CHUNKED_FUNC_SIZE, (func_id + 1) * CHUNKED_FUNC_SIZE);
  inc_indent();
  emit_line("pc, WTF?");
  inc_indent();
}

static void lol_emit_func_epilogue(void) {
  dec_indent();
  emit_line("OIC");
  emit_line("pc R SUM OF pc AN 1");
  dec_indent();
  dec_indent();
  emit_line("IF U SAY SO");
}

static void lol_emit_pc_change(int pc) {
  emit_line("");
  dec_indent();
  emit_line("OMG %d", pc);
  inc_indent();
}

static void lol_emit_inst(Inst* inst) {
  switch (inst->op) {
  case MOV:
    emit_line("%s R %s", reg_names[inst->dst.reg], src_str(inst));
    break;

  case ADD:
    emit_line("%s R SMALLR OF SUM OF %s AN %s AN " UINT_MAX_STR,
              reg_names[inst->dst.reg],
              reg_names[inst->dst.reg], src_str(inst));
    break;

  case SUB:
    emit_line("%s R SMALLR OF DIFF OF %s AN %s AN " UINT_MAX_STR,
              reg_names[inst->dst.reg],
              reg_names[inst->dst.reg], src_str(inst));
    break;

  case LOAD:
    emit_line("%s R mem'Z m%s", reg_names[inst->dst.reg], src_str(inst));
    break;

  case STORE:
    emit_line("mem'Z m%s R %s", src_str(inst), reg_names[inst->dst.reg]);
    break;

  case PUTC:
    emit_line("VISIBLE %s", src_str(inst));
    break;

  case GETC:
    emit_line("I HAS A p, GIMMEH p, %s R p",
              reg_names[inst->dst.reg]);
    break;

  case EXIT:
    // TODO: add lolcode exit
    emit_line("sys.exit(0)");
    break;

  case DUMP:
    break;

  case EQ:
  case NE:
  case LE:
  case GE:
  case LT:
  case GT:
    emit_line("%s = MAEK %s A NUMBAR",
              lol_cmp_str(inst->op, regnames[inst->dst.reg], regnames[inst->src.reg]));
    break;

  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
  case JMP:
    emit_line("if %s: pc = %s - 1",
              lol_cmp_str(inst->op, regnames[inst->dst.reg], regnames[inst->src.reg]), value_str(&inst->jmp));
    break;

  default:
    error("oops");
  }
}

void target_lol(Module* module) {
  init_state_py(module->data);

  int num_funcs = emit_chunked_main_loop(module->text,
                                         lol_emit_func_prologue,
                                         lol_emit_func_epilogue,
                                         lol_emit_pc_change,
                                         lol_emit_inst);

  emit_line("");
  emit_line("while True:");
  inc_indent();
  emit_line("if False: pass");
  for (int i = 0; i < num_funcs; i++) {
    emit_line("elif pc < %d: func%d()", (i + 1) * CHUNKED_FUNC_SIZE, i);
  }
  dec_indent();
}
