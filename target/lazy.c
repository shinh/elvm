#include <ir/ir.h>
#include <target/util.h>
#include <target/lazycore.h>



static const int LAZY_N_BITS = 24;
static const char LAZY_8[] = "``s``s`ks``s`kk``s`k``s``s`ks``s`kki``s``s`ks``s`kki`ki``s`k```sii``s``s`ks``s`kki``s``s`ks``s`kki`kii`ki";
static const char LAZY_16[] = "```s``siii``s``s`ks``s`kki``s``s`ks``s`kki`ki";

static const char LAZY_APPLY[] = "`";
static const char LAZY_T[] = "k";
static const char LAZY_NIL[] = "`ki";

// (cons x y) = (lambda (f) (f x y))
// = ``s``si`k[x]`k[y]
static const char LAZY_CONS_HEAD[] = "``s``si`k";
static const char LAZY_CONS_COMMA[] = "`k";

// (cons4 x1 x2 x3 x4) = (lambda (f) (f x1 x2 x3 x4))
// = ``s``s``s``si`k[x1]`k[x2]`k[x3]`k[x4]
static const char LAZY_CONS4_HEAD[] = "``s``s``s``si`k";

static const char LAZY_REG_A[]  = "``s``si`kk`k`ki";
static const char LAZY_REG_B[]  = "``s``si`k`ki`k``s``si`kk`k``s``si`kk`k`ki";
static const char LAZY_REG_C[]  = "``s``si`k`ki`k``s``si`k`ki`k``s``si`k`ki`k``s``si`k`ki`k`ki";
static const char LAZY_REG_D[]  = "``s``si`k`ki`k``s``si`k`ki`k``s``si`kk`k`ki";
static const char LAZY_REG_SP[] = "``s``si`k`ki`k``s``si`kk`k``s``si`k`ki`k`ki";
static const char LAZY_REG_BP[] = "``s``si`k`ki`k``s``si`k`ki`k``s``si`k`ki`k``s``si`kk`k`ki";

static const char LAZY_INST_IO[] = "``s`kk``s`kk``s`kk``s`kk``s`kk``s`kkk";
static const char LAZY_INST_JMPCMP[] = "`k``s`kk``s`kk``s`kk``s`kk``s`kkk";
static const char LAZY_INST_CMP[] = "`k`k``s`kk``s`kk``s`kk``s`kkk";
static const char LAZY_INST_JMP[] = "`k`k`k``s`kk``s`kk``s`kkk";
static const char LAZY_INST_LOAD[] = "`k`k`k`k``s`kk``s`kkk";
static const char LAZY_INST_STORE[] = "`k`k`k`k`k``s`kkk";
static const char LAZY_INST_ADDSUB[] = "`k`k`k`k`k`kk";
static const char LAZY_INST_MOV[] = "`k`k`k`k`k`k`ki";

static const char LAZY_CMP_GT[] = "``s``s``si`k`ki`k`ki`kk";
static const char LAZY_CMP_LT[] = "``s``s``si`k`ki`kk`k`ki";
static const char LAZY_CMP_EQ[] = "``s``s``si`kk`k`ki`k`ki";
static const char LAZY_CMP_LE[] = "``s``s``si`kk`kk`k`ki";
static const char LAZY_CMP_GE[] = "``s``s``si`kk`k`ki`kk";
static const char LAZY_CMP_NE[] = "``s``s``si`k`ki`kk`kk";

static const char LAZY_IO_GETC[] = "``s`kkk";
static const char LAZY_IO_PUTC[] = "`kk";
static const char LAZY_IO_EXIT[] = "`k`ki";


static const char* lazy_reg(Reg r) {
  switch (r) {
  case A: return LAZY_REG_A;
  case B: return LAZY_REG_B;
  case C: return LAZY_REG_C;
  case D: return LAZY_REG_D;
  case BP: return LAZY_REG_BP;
  case SP: return LAZY_REG_SP;
  default:
    error("unknown register: %d", r);
  }
}

static void lazy_emit_int(int n) {
#ifndef __eir__
    n &= ((1 << LAZY_N_BITS) - 1);
#endif
  for (int checkbit = 1 << (LAZY_N_BITS - 1); checkbit; checkbit >>= 1) {
    fputs(LAZY_CONS_HEAD, stdout);
    fputs((n & checkbit) ? LAZY_NIL : LAZY_T, stdout);
    fputs(LAZY_CONS_COMMA, stdout);
  }
  fputs(LAZY_NIL, stdout);
}

static void lazy_emit_isimm(Value* v) {
  if (v->type == REG) {
    fputs(LAZY_NIL, stdout);
  } else if (v->type == IMM) {
    fputs(LAZY_T, stdout);
  } else {
    error("invalid value");
  }
}

static void lazy_emit_value_str(Value* v) {
  if (v->type == REG) {
    fputs(lazy_reg(v->reg), stdout);
  } else if (v->type == IMM) {
    lazy_emit_int(v->imm);
  } else {
    error("invalid value");
  }
}

static void lazy_emit_basic_inst(Inst* inst, const char* inst_tag) {
    fputs(LAZY_CONS4_HEAD, stdout);
    fputs(inst_tag, stdout);
    fputs(LAZY_CONS_COMMA, stdout);
    lazy_emit_isimm(&inst->src);
    fputs(LAZY_CONS_COMMA, stdout);
    lazy_emit_value_str(&inst->src);
    fputs(LAZY_CONS_COMMA, stdout);
    lazy_emit_value_str(&inst->dst);
}

static void lazy_emit_addsub_inst(Inst* inst, const char* isadd) {
    fputs(LAZY_CONS4_HEAD, stdout);
    fputs(LAZY_INST_ADDSUB, stdout);
    fputs(LAZY_CONS_COMMA, stdout);
    lazy_emit_isimm(&inst->src);
    fputs(LAZY_CONS_COMMA, stdout);
    lazy_emit_value_str(&inst->src);
    fputs(LAZY_CONS_COMMA, stdout);
    fputs(LAZY_CONS_HEAD, stdout);
    lazy_emit_value_str(&inst->dst);
    fputs(LAZY_CONS_COMMA, stdout);
    fputs(isadd, stdout);
}

static void lazy_emit_jumpcmp_inst(Inst* inst, const char* cmp_tag) {
    fputs(LAZY_CONS4_HEAD, stdout);
    fputs(LAZY_INST_JMPCMP, stdout);
    fputs(LAZY_CONS_COMMA, stdout);
    lazy_emit_isimm(&inst->src);
    fputs(LAZY_CONS_COMMA, stdout);
    lazy_emit_value_str(&inst->src);
    fputs(LAZY_CONS_COMMA, stdout);
    fputs(LAZY_CONS4_HEAD, stdout);
    fputs(cmp_tag, stdout);
    fputs(LAZY_CONS_COMMA, stdout);
    lazy_emit_isimm(&inst->jmp);
    fputs(LAZY_CONS_COMMA, stdout);
    lazy_emit_value_str(&inst->jmp);
    fputs(LAZY_CONS_COMMA, stdout);
    lazy_emit_value_str(&inst->dst);
}

static void lazy_emit_cmp_inst(Inst* inst, const char* cmp_tag) {
    fputs(LAZY_CONS4_HEAD, stdout);
    fputs(LAZY_INST_CMP, stdout);
    fputs(LAZY_CONS_COMMA, stdout);
    lazy_emit_isimm(&inst->src);
    fputs(LAZY_CONS_COMMA, stdout);
    lazy_emit_value_str(&inst->src);
    fputs(LAZY_CONS_COMMA, stdout);
    fputs(LAZY_CONS_HEAD, stdout);
    fputs(cmp_tag, stdout);
    fputs(LAZY_CONS_COMMA, stdout);
    lazy_emit_value_str(&inst->dst);
}


static void lazy_emit_inst(Inst* inst) {
  switch (inst->op) {
  case MOV: lazy_emit_basic_inst(inst, LAZY_INST_MOV); break;
  case LOAD: lazy_emit_basic_inst(inst, LAZY_INST_LOAD); break;
  case STORE: lazy_emit_basic_inst(inst, LAZY_INST_STORE); break;

  case ADD: lazy_emit_addsub_inst(inst, LAZY_T); break;
  case SUB: lazy_emit_addsub_inst(inst, LAZY_NIL); break;

  case EQ: lazy_emit_cmp_inst(inst, LAZY_CMP_EQ); break;
  case NE: lazy_emit_cmp_inst(inst, LAZY_CMP_NE); break;
  case LT: lazy_emit_cmp_inst(inst, LAZY_CMP_LT); break;
  case GT: lazy_emit_cmp_inst(inst, LAZY_CMP_GT); break;
  case LE: lazy_emit_cmp_inst(inst, LAZY_CMP_LE); break;
  case GE: lazy_emit_cmp_inst(inst, LAZY_CMP_GE); break;

  case JEQ: lazy_emit_jumpcmp_inst(inst, LAZY_CMP_EQ); break;
  case JNE: lazy_emit_jumpcmp_inst(inst, LAZY_CMP_NE); break;
  case JLT: lazy_emit_jumpcmp_inst(inst, LAZY_CMP_LT); break;
  case JGT: lazy_emit_jumpcmp_inst(inst, LAZY_CMP_GT); break;
  case JLE: lazy_emit_jumpcmp_inst(inst, LAZY_CMP_LE); break;
  case JGE: lazy_emit_jumpcmp_inst(inst, LAZY_CMP_GE); break;

  case JMP:
    fputs(LAZY_CONS4_HEAD, stdout);
    fputs(LAZY_INST_JMP, stdout);
    fputs(LAZY_CONS_COMMA, stdout);
    lazy_emit_isimm(&inst->jmp);
    fputs(LAZY_CONS_COMMA, stdout);
    lazy_emit_value_str(&inst->jmp);
    fputs(LAZY_CONS_COMMA, stdout);
    fputs(LAZY_NIL, stdout);
    break;

  case PUTC:
    fputs(LAZY_CONS4_HEAD, stdout);
    fputs(LAZY_INST_IO, stdout);
    fputs(LAZY_CONS_COMMA, stdout);
    lazy_emit_isimm(&inst->src);
    fputs(LAZY_CONS_COMMA, stdout);
    lazy_emit_value_str(&inst->src);
    fputs(LAZY_CONS_COMMA, stdout);
    fputs(LAZY_IO_PUTC, stdout);
    break;

  case GETC:
    fputs(LAZY_CONS4_HEAD, stdout);
    fputs(LAZY_INST_IO, stdout);
    fputs(LAZY_CONS_COMMA, stdout);
    fputs(LAZY_NIL, stdout);
    fputs(LAZY_CONS_COMMA, stdout);
    lazy_emit_value_str(&inst->dst);
    fputs(LAZY_CONS_COMMA, stdout);
    fputs(LAZY_IO_GETC, stdout);
    break;

  case EXIT:
    fputs(LAZY_CONS4_HEAD, stdout);
    fputs(LAZY_INST_IO, stdout);
    fputs(LAZY_CONS_COMMA, stdout);
    fputs(LAZY_NIL, stdout);
    fputs(LAZY_CONS_COMMA, stdout);
    fputs(LAZY_NIL, stdout);
    fputs(LAZY_CONS_COMMA, stdout);
    fputs(LAZY_IO_EXIT, stdout);
    break;

  case DUMP:
    fputs(LAZY_CONS4_HEAD, stdout);
    fputs(LAZY_INST_MOV, stdout);
    fputs(LAZY_CONS_COMMA, stdout);
    fputs(LAZY_NIL, stdout);
    fputs(LAZY_CONS_COMMA, stdout);
    fputs(LAZY_REG_A, stdout);
    fputs(LAZY_CONS_COMMA, stdout);
    fputs(LAZY_REG_A, stdout);
    break;

  default:
    error("oops");
  }
}

static Inst* lazy_emit_chunk(Inst* inst) {
  const int init_pc = inst->pc;
  for (; inst && (inst->pc == init_pc); inst = inst->next) {
    fputs("\n  ", stdout);
    fputs(LAZY_CONS_HEAD, stdout);
    lazy_emit_inst(inst);
    fputs(LAZY_CONS_COMMA, stdout);
  }
  fputs(LAZY_NIL, stdout);
  return inst;
}

static void lazy_emit_data_list(Data* data) {
  for (; data; data = data->next){
    fputs("\n  ", stdout);
    fputs(LAZY_CONS_HEAD, stdout);
    lazy_emit_int(data->v);
    fputs(LAZY_CONS_COMMA, stdout);
  }
  fputs(LAZY_NIL, stdout);
}

static void lazy_emit_text_list(Inst* inst) {
  while (inst) {
    putchar('\n');
    fputs(LAZY_CONS_HEAD, stdout);
    inst = lazy_emit_chunk(inst);
    fputs(LAZY_CONS_COMMA, stdout);
  }
  fputs(LAZY_NIL, stdout);
}

void target_lazy(Module* module) {
  fputs(LAZY_APPLY, stdout);
  fputs(LAZY_APPLY, stdout);
  fputs(LAZY_APPLY, stdout);
  fputs(LAZY_APPLY, stdout);
  fputs(LAZY_VM, stdout);
  fputs(LAZY_8, stdout);
  fputs(LAZY_16, stdout);
  lazy_emit_data_list(module->data);
  lazy_emit_text_list(module->text);
}
