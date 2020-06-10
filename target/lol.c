#include <ir/ir.h>
#include <target/util.h>

static void init_state_lol(Data* data) {
  emit_line("HAI 1.4");
  emit_line("CAN HAS STDIO?");

  // Bitwise and //
  emit_line("HOW IZ I exp2 YR exp");
  emit_line("  I HAS A out ITZ 1");
  emit_line("  I HAS A i ITZ 0");
  emit_line("  IM IN YR exploop UPPIN YR i TIL BOTH SAEM i AN exp");
  emit_line("    out R PRODUKT OF 2 AN out");
  emit_line("  IM OUTTA YR exploop");
  emit_line("  FOUND YR out");
  emit_line("IF U SAY SO");

  emit_line("HOW IZ I bitwise_and YR a AN YR b");
  emit_line("  I HAS A cpya ITZ a");
  emit_line("  I HAS A cpyb ITZ b");
  emit_line("  I HAS A out ITZ 0");
  emit_line("  I HAS A biton ITZ 0");
  emit_line("  IM IN YR andloop UPPIN YR biton TIL BOTH SAEM cpyb AN 0");
  emit_line("    BOTH OF BOTH SAEM MOD OF cpyb AN 2 AN MOD OF cpya AN 2 AN BOTH SAEM MOD OF cpyb AN 2 AN 1, O RLY?");
  emit_line("      YA RLY");
  emit_line("        out R SUM OF out AN I IZ exp2 YR biton MKAY");
  emit_line("    OIC");

  emit_line("    cpya R QUOSHUNT OF cpya AN 2");
  emit_line("    cpyb R QUOSHUNT OF cpyb AN 2");

  emit_line("  IM OUTTA YR andloop");
  emit_line("  FOUND YR out");
  emit_line("IF U SAY SO");

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

static char* lol_cmp_str(Inst* inst){
  switch (inst->op){
    case EQ:
    case JEQ:
      return format("BOTH SAEM %s AN %s", reg_names[inst->dst.reg], src_str(inst));
    case NE:
    case JNE:
      return format("DIFFRINT %s AN %s", reg_names[inst->dst.reg], src_str(inst));
    case LE:
    case JLE:
      return format("BOTH SAEM %1$s AN SMALLER OF %1$s AN %2$s", reg_names[inst->dst.reg], src_str(inst));
    case GE:
    case JGE:
      return format("BOTH SAEM %1$s AN BIGGR OF %1$s AN %2$s", reg_names[inst->dst.reg], src_str(inst));
    case GT:
    case JGT:
      return format("DIFFRINT %1$s AN SMALLER OF %1$s AN %2$s", reg_names[inst->dst.reg], src_str(inst));
    case LT:
    case JLT:
      return format("DIFFRINT %1$s AN BIGGR OF %1$s AN %2$s", reg_names[inst->dst.reg], src_str(inst));
    default:
      return "WIN";
  }
}

static void lol_emit_func_prologue(int func_id) {
  emit_line("HOW IZ I func%d", func_id);
  inc_indent();
  emit_line("");

  emit_line("IM IN YR loop UPPIN YR pc TIL BOTH OF BOTH SAEM %1$d AN SMALLER OF %1$d AN pc AN DIFFRINT pc AN BIGGR OF pc AN %2$d",
      func_id * CHUNKED_FUNC_SIZE, (func_id + 1) * CHUNKED_FUNC_SIZE);
  inc_indent();
  emit_line("pc, WTF?");
  inc_indent();
}

static void lol_emit_func_epilogue(void) {
  dec_indent();
  emit_line("OIC");
  dec_indent();
  emit_line("IM OUTTA YR loop");
  dec_indent();
  emit_line("IF U SAY SO");
}

static void lol_emit_pc_change(int pc) {
  emit_line("GTFO");
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
    emit_line("%s R IZ bitwise_and YR SUM OF %s AN %s AN YR %d MKAY",
              reg_names[inst->dst.reg],
              reg_names[inst->dst.reg], src_str(inst),
              UINT_MAX_STR);
    break;

  case SUB:
    emit_line("%s R IZ bitwise_and YR DIFF OF %s AN %s AN YR %d MKAY",
              reg_names[inst->dst.reg],
              reg_names[inst->dst.reg], src_str(inst),
              UINT_MAX_STR);
    break;

  case LOAD:
    emit_line("%s R mem'Z m%s", reg_names[inst->dst.reg], src_str(inst));
    break;

  case STORE:
    emit_line("mem'Z m%s R %s", src_str(inst), reg_names[inst->dst.reg]);
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
    emit_line("%s R MAEK %s A NUMR",
              reg_names[inst->dst.reg], lol_cmp_str(inst));
    break;

  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
  case JMP:
    emit_line("%s, O RLY?", lol_cmp_str(inst));
    inc_indent();
    emit_line("YA RLY");
    inc_indent();
    emit_line("pc R DIFF OF %s AN 1", value_str(&inst->jmp));
    dec_indent();
    dec_indent();
    emit_line("OIC");
    break;

  default:
    error("oops");
  }
}

void target_lol(Module* module) {
  init_state_lol(module->data);

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
