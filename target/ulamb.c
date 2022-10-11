#include <ir/ir.h>
#include <target/util.h>
#include <target/ulambcore.h>


static const int ULAMB_N_BITS = 24;
static const char ULAMB_16[] = "010001011010100000011100111010";
static const char ULAMB_8[] = "0000011100111001110011100111001110011100111010";

static const char ULAMB_APPLY[] = "01";

// (cons x y) = (lambda (f) (f x y)) = 00010110[x][y]
static const char ULAMB_CONS_HEAD[] = "00010110";

// (cons4 x1 x2 x3 x4) = (lambda (f) (f x1 x2 x3 x4)) = 000101010110[x1][x2][x3][x4]
static const char ULAMB_CONS4_HEAD[] = "000101010110";

// ((lambda (cons-t cons-nil) [A]) (lambda (x f) (f t x)) (lambda (x f) (f nil x)))
//   = 01000100[A]000001011000001011000000101100000110110
// Where [A] is
// (F3 (F2 (F1 ULAMB_NIL)))
//   = 01[F3]01[F2]01[F1]000010
// Where F1, F2, ... is in { 10, 110 }
static const char ULAMB_INT_HEADER[] = "01000100";
static const char ULAMB_INT_BIT1[] = "10";
static const char ULAMB_INT_BIT0[] = "110";
static const char ULAMB_INT_FOOTER[] = "000010000001011000001011000000101100000110110";

static const char ULAMB_T[] = "0000110";
static const char ULAMB_NIL[] = "000010";

static const char ULAMB_REG_A[]  = "00010110000010000101100000110000010";
static const char ULAMB_REG_B[]  = "00010110000011000010110000011000010110000010000010";
static const char ULAMB_REG_C[]  = "00010110000011000010110000011000010110000011000010110000010000010";
static const char ULAMB_REG_D[]  = "0001011000001100001011000001000010110000010000010";
static const char ULAMB_REG_SP[] = "00010110000011000010110000010000101100000110000010";
static const char ULAMB_REG_BP[] = "01010100011010000001110011101000000101100000110110000010";
static const char ULAMB_INST_MOV[] = "000000000000000010";
static const char ULAMB_INST_ADDSUB[] = "0000000000000000110";
static const char ULAMB_INST_STORE[] = "00000000000000001110";
static const char ULAMB_INST_LOAD[] = "000000000000000011110";
static const char ULAMB_INST_JMP[] = "0000000000000000111110";
static const char ULAMB_INST_CMP[] = "00000000000000001111110";
static const char ULAMB_INST_JMPCMP[] = "000000000000000011111110";
static const char ULAMB_INST_IO[] = "0000000000000000111111110";
static const char ULAMB_CMP_GT[] = "00010101100000100000100000110";
static const char ULAMB_CMP_LT[] = "00010101100000100000110000010";
static const char ULAMB_CMP_EQ[] = "00010101100000110000010000010";
static const char ULAMB_CMP_LE[] = "000101011000001100000110000010";
static const char ULAMB_CMP_GE[] = "000101011000001100000100000110";
static const char ULAMB_CMP_NE[] = "000101011000001000001100000110";
static const char ULAMB_IO_GETC[] = "0000001110";
static const char ULAMB_IO_PUTC[] = "000000110";
static const char ULAMB_IO_EXIT[] = "00000010";
static const char ULAMB_PLACEHOLDER[] = "10";

static const char* ulamb_reg(Reg r) {
  switch (r) {
  case A: return ULAMB_REG_A;
  case B: return ULAMB_REG_B;
  case C: return ULAMB_REG_C;
  case D: return ULAMB_REG_D;
  case BP: return ULAMB_REG_BP;
  case SP: return ULAMB_REG_SP;
  default:
    error("unknown register: %d", r);
  }
}

static void ulamb_emit_int(int n) {
#ifndef __eir__
  n &= ((1 << ULAMB_N_BITS) - 1);
#endif
  fputs(ULAMB_INT_HEADER, stdout);
  for (int checkbit = 1 << (ULAMB_N_BITS - 1); checkbit; checkbit >>= 1) {
    fputs(ULAMB_APPLY, stdout);
    fputs((n & checkbit) ? ULAMB_INT_BIT1 : ULAMB_INT_BIT0, stdout);
  }
  fputs(ULAMB_INT_FOOTER, stdout);
}

static void emit_ulamb_isimm(Value* v) {
  if (v->type == REG) {
    fputs(ULAMB_NIL, stdout);
  } else if (v->type == IMM) {
    fputs(ULAMB_T, stdout);
  } else {
    error("invalid value");
  }
}

static void emit_ulamb_value_str(Value* v) {
  if (v->type == REG) {
    fputs(ulamb_reg(v->reg), stdout);
  } else if (v->type == IMM) {
    ulamb_emit_int(v->imm);
  } else {
    error("invalid value");
  }
}

static void ulamb_emit_data_list(Data* data) {
  for (; data; data = data->next){
    fputs(ULAMB_CONS_HEAD, stdout);
    ulamb_emit_int(data->v);
  }
  fputs(ULAMB_NIL, stdout);
}

static void ulamb_emit_inst_header(const char* inst_tag, Value* v) {
  fputs(ULAMB_CONS4_HEAD, stdout);
  fputs(inst_tag, stdout);
  emit_ulamb_isimm(v);
  emit_ulamb_value_str(v);
}

static void ulamb_emit_basic_inst(Inst* inst, const char* inst_tag) {
  ulamb_emit_inst_header(inst_tag, &inst->src);
  emit_ulamb_value_str(&inst->dst);
}

static void ulamb_emit_addsub_inst(Inst* inst, const char* is_add) {
  ulamb_emit_inst_header(ULAMB_INST_ADDSUB, &inst->src);
  fputs(ULAMB_CONS_HEAD, stdout);
  emit_ulamb_value_str(&inst->dst);
  fputs(is_add, stdout);
}

static void ulamb_emit_jmpcmp_inst(Inst* inst, const char* cmp_tag) {
  ulamb_emit_inst_header(ULAMB_INST_JMPCMP, &inst->src);
  ulamb_emit_inst_header(cmp_tag, &inst->jmp);
  emit_ulamb_value_str(&inst->dst);
}

static void ulamb_emit_cmp_inst(Inst* inst, const char* cmp_tag) {
  ulamb_emit_inst_header(ULAMB_INST_CMP, &inst->src);
  fputs(ULAMB_CONS_HEAD, stdout);
  fputs(cmp_tag, stdout);
  emit_ulamb_value_str(&inst->dst);
}

static void ulamb_emit_io_inst(const char* io_tag, Value* v) {
  ulamb_emit_inst_header(ULAMB_INST_IO, v);
  fputs(io_tag, stdout);
}

static void ulamb_emit_exit_inst() {
  fputs(ULAMB_CONS4_HEAD, stdout);
  fputs(ULAMB_INST_IO, stdout);
  fputs(ULAMB_NIL, stdout);
  fputs(ULAMB_NIL, stdout);
  fputs(ULAMB_IO_EXIT, stdout);
}

static void ulamb_emit_jmp_inst(Inst* inst) {
  ulamb_emit_inst_header(ULAMB_INST_JMP, &inst->jmp);
  fputs(ULAMB_PLACEHOLDER, stdout);
}

static void ulamb_emit_dump_inst(void) {
  fputs(ULAMB_CONS4_HEAD, stdout);
  fputs(ULAMB_INST_MOV, stdout);
  fputs(ULAMB_NIL, stdout);
  fputs(ULAMB_REG_A, stdout);
  fputs(ULAMB_REG_A, stdout);
}

static void ulamb_emit_inst(Inst* inst) {
  switch (inst->op) {
  case MOV: ulamb_emit_basic_inst(inst, ULAMB_INST_MOV); break;
  case LOAD: ulamb_emit_basic_inst(inst, ULAMB_INST_LOAD); break;
  case STORE: ulamb_emit_basic_inst(inst, ULAMB_INST_STORE); break;

  case ADD: ulamb_emit_addsub_inst(inst, ULAMB_T); break;
  case SUB: ulamb_emit_addsub_inst(inst, ULAMB_NIL); break;

  case EQ: ulamb_emit_cmp_inst(inst, ULAMB_CMP_EQ); break;
  case NE: ulamb_emit_cmp_inst(inst, ULAMB_CMP_NE); break;
  case LT: ulamb_emit_cmp_inst(inst, ULAMB_CMP_LT); break;
  case GT: ulamb_emit_cmp_inst(inst, ULAMB_CMP_GT); break;
  case LE: ulamb_emit_cmp_inst(inst, ULAMB_CMP_LE); break;
  case GE: ulamb_emit_cmp_inst(inst, ULAMB_CMP_GE); break;

  case JEQ: ulamb_emit_jmpcmp_inst(inst, ULAMB_CMP_EQ); break;
  case JNE: ulamb_emit_jmpcmp_inst(inst, ULAMB_CMP_NE); break;
  case JLT: ulamb_emit_jmpcmp_inst(inst, ULAMB_CMP_LT); break;
  case JGT: ulamb_emit_jmpcmp_inst(inst, ULAMB_CMP_GT); break;
  case JLE: ulamb_emit_jmpcmp_inst(inst, ULAMB_CMP_LE); break;
  case JGE: ulamb_emit_jmpcmp_inst(inst, ULAMB_CMP_GE); break;

  case JMP: ulamb_emit_jmp_inst(inst); break;

  case PUTC: ulamb_emit_io_inst(ULAMB_IO_PUTC, &inst->src); break;
  case GETC: ulamb_emit_io_inst(ULAMB_IO_GETC, &inst->dst); break;

  case EXIT: ulamb_emit_exit_inst(); break;
  case DUMP: ulamb_emit_dump_inst(); break;

  default:
    error("oops");
  }
}

static Inst* ulamb_emit_chunk(Inst* inst) {
  const int init_pc = inst->pc;
  for (; inst && (inst->pc == init_pc); inst = inst->next) {
    fputs(ULAMB_CONS_HEAD, stdout);
    ulamb_emit_inst(inst);
  }
  fputs(ULAMB_NIL, stdout);
  return inst;
}

static void ulamb_emit_text_list(Inst* inst) {
  while (inst) {
    fputs(ULAMB_CONS_HEAD, stdout);
    inst = ulamb_emit_chunk(inst);
  }
  fputs(ULAMB_NIL, stdout);
}

void target_ulamb(Module* module) {
  fputs(ULAMB_APPLY, stdout);
  fputs(ULAMB_APPLY, stdout);
  fputs(ULAMB_APPLY, stdout);
  fputs(ULAMB_APPLY, stdout);
  fputs(ULAMB_CORE, stdout);
  fputs(ULAMB_8, stdout);
  fputs(ULAMB_16, stdout);
  ulamb_emit_data_list(module->data);
  ulamb_emit_text_list(module->text);
}
