#include <ir/ir.h>
#include <target/util.h>

static void init_state_lol(Data* data) {
  emit_line("HAI 1.4");
  emit_line("CAN HAS STRING?");
  emit_line("I HAS A running ITZ WIN");
  emit_line("I HAS A buffer ITZ A YARN");
  emit_line("I HAS A bufferptr ITZ 0");

  // Array functions
  emit_line("I HAS A mem ITZ A BUKKIT");
  emit_line("I HAS A slotsused ITZ A BUKKIT");
  emit_line("I HAS A numslots ITZ 0");

  emit_line("HOW IZ I indexUsed YR val");
  emit_line("  IM IN YR loop UPPIN YR i TIL BOTH SAEM i AN numslots");
  emit_line("    BOTH SAEM val AN slotsused'Z SRS \"s:{i}\", O RLY?");
  emit_line("      YA RLY");
  emit_line("        FOUND YR WIN");
  emit_line("    OIC");
  emit_line("  IM OUTTA YR loop");
  emit_line("  FOUND YR FAIL");
  emit_line("IF U SAY SO");

  emit_line("HOW IZ I addUsed YR loc");
  emit_line("  slotsused HAS A SRS \"s:{numslots}\" ITZ loc ");
  emit_line("  numslots R SUM OF numslots AN 1");
  emit_line("IF U SAY SO");

  emit_line("HOW IZ I getMem YR loc");
  emit_line("  DIFFRINT loc AN SMALLR OF loc AN %s, O RLY?", UINT_MAX_STR);
  emit_line("    YA RLY");
  emit_line("      FOUND YR 0");
  emit_line("  OIC");
  emit_line("  I IZ indexUsed YR loc MKAY, O RLY?");
  emit_line("    YA RLY");
  emit_line("      FOUND YR mem'Z SRS \"m:{loc}\"");
  emit_line("    NO WAI");
  emit_line("      FOUND YR 0");
  emit_line("  OIC");
  emit_line("IF U SAY SO");


  emit_line("HOW IZ I setMem YR loc AN YR val");
  emit_line("  DIFFRINT loc AN SMALLR OF loc AN %s, O RLY?", UINT_MAX_STR);
  emit_line("    YA RLY");
  emit_line("      GTFO");
  emit_line("  OIC");
  emit_line("  I IZ indexUsed YR loc MKAY, O RLY?");
  emit_line("    YA RLY");
  emit_line("      mem'Z SRS \"m:{loc}\" R val");
  emit_line("    NO WAI");
  emit_line("      I IZ addUsed YR loc MKAY");
  emit_line("      mem HAS A SRS \"m:{loc}\" ITZ val");
  emit_line("  OIC");
  emit_line("IF U SAY SO");

  // number to char

  emit_line("HOW IZ I num2char YR n");
  inc_indent();
  emit_line("n, WTF?");
  inc_indent();
  
  for (int n = 0; n < 256; n++){
    emit_line("OMG %d", n);
    inc_indent();
    emit_line("FOUND YR \":(%x)\"", n);
    dec_indent();
  }

  dec_indent();
  emit_line("OIC");
  dec_indent();
  emit_line("IF U SAY SO");

  // char to number

  emit_line("HOW IZ I char2num YR c");
  inc_indent();
  emit_line("c, WTF?");
  inc_indent();
  
  for (int n = 0; n < 256; n++){
    emit_line("OMG \":(%x)\"", n);
    inc_indent();
    emit_line("FOUND YR %d", n);
    dec_indent();
  }

  dec_indent();
  emit_line("OIC");
  dec_indent();
  emit_line("IF U SAY SO");

  for (int i = 0; i < 7; i++) {
    emit_line("I HAS A %s ITZ 0", reg_names[i]);
  }

  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      emit_line("I IZ setMem YR %d AN YR %d MKAY", mp, data->v);
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
      return format("BOTH SAEM %1$s AN SMALLR OF %1$s AN %2$s", reg_names[inst->dst.reg], src_str(inst));
    case GE:
    case JGE:
      return format("BOTH SAEM %1$s AN BIGGR OF %1$s AN %2$s", reg_names[inst->dst.reg], src_str(inst));
    case GT:
    case JGT:
      return format("DIFFRINT %1$s AN SMALLR OF %1$s AN %2$s", reg_names[inst->dst.reg], src_str(inst));
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

  emit_line("IM IN YR loop UPPIN YR temp0 WILE BOTH OF BOTH SAEM %1$d AN SMALLR OF %1$d AN pc AN DIFFRINT pc AN BIGGR OF pc AN %2$d",
      func_id * CHUNKED_FUNC_SIZE, (func_id + 1) * CHUNKED_FUNC_SIZE);
  inc_indent();
  emit_line("pc, WTF?");
  inc_indent();
  emit_line("OMG -1");
  inc_indent();
}

static void lol_emit_func_epilogue(void) {
  dec_indent();
  dec_indent();
  emit_line("OIC");
  emit_line("pc R SUM OF pc AN 1");
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
    emit_line("%s R MOD OF SUM OF %s AN %s AN %s",
              reg_names[inst->dst.reg],
              reg_names[inst->dst.reg],
              src_str(inst), UINT_MAX_STR);
    break;

  case SUB:
    emit_line("%s R MOD OF DIFF OF %s AN %s AN %s",
              reg_names[inst->dst.reg],
              reg_names[inst->dst.reg],
              src_str(inst), UINT_MAX_STR);
    break;

  case LOAD:
    emit_line("%s R I IZ getMem YR %s MKAY", reg_names[inst->dst.reg], src_str(inst));
    break;

  case STORE:
    emit_line("I IZ setMem YR %s AN YR %s MKAY", src_str(inst), reg_names[inst->dst.reg]);
    break;

  case PUTC:
    emit_line("VISIBLE I IZ num2char YR %s MKAY!", src_str(inst));
    break;

  case GETC:

    emit_line("buffer R SMOOSH buffer AN temp MKAY");
    emit_line("bufferptr R SUM OF bufferptr AN 1");
    emit_line("BOTH SAEM bufferptr AN I IZ STRING'Z LEN YR buffer MKAY, O RLY?");
    inc_indent();
    emit_line("YA RLY");
    inc_indent();
    emit_line("bufferptr R 0");
    emit_line("GIMMEH buffer");
    dec_indent();
    dec_indent();
    emit_line("OIC");

    emit_line("%s R I IZ char2num YR I IZ STRING'Z AT YR buffer AN YR bufferptr MKAY MKAY",
              reg_names[inst->dst.reg]);


    break;

  case EXIT:
    emit_line("running R FAIL");
    emit_line("GTFO");
    break;

  case DUMP:
    break;

  case EQ:
  case NE:
  case LT:
  case GT:
  case LE:
  case GE:
    emit_line("%s R MAEK %s A NUMBR",
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
  emit_line("IM IN YR mainloop UPPIN YR temp WILE running");
  inc_indent();
  emit_line("FAIL, O RLY?");
  inc_indent();
  emit_line("YA RLY");
  for (int i = 0; i < num_funcs; i++) {
    emit_line("MEBBE BOTH SAEM pc AN SMALLR OF pc AN %d", (i + 1) * CHUNKED_FUNC_SIZE);
    inc_indent();
    emit_line("I IZ func%d MKAY", i);
    dec_indent();
  }
  emit_line("OIC");
  dec_indent();
  emit_line("IM OUTTA YR mainloop");
  dec_indent();
  emit_line("KTHXBYE");
}
