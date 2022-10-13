#include <ir/ir.h>
#include <target/util.h>
#include <target/wcore.h>


#define GRASS_N_BITS 24

#define GRASS_INST_MOV 8
#define GRASS_INST_ADDSUB 7
#define GRASS_INST_STORE 6
#define GRASS_INST_LOAD 5
#define GRASS_INST_JMP 4
#define GRASS_INST_CMP 3
#define GRASS_INST_JMPCMP 2
#define GRASS_INST_IO 1

#define GRASS_IO_GETC 1
#define GRASS_IO_PUTC 2
#define GRASS_IO_EXIT 3

#define GRASS_NIL 0
#define GRASS_T 1

#define GRASS_REG_A_WEIGHT 4
#define GRASS_REG_B_WEIGHT 6
#define GRASS_REG_C_WEIGHT 6
#define GRASS_REG_D_WEIGHT 6
#define GRASS_REG_SP_WEIGHT 6
#define GRASS_REG_BP_WEIGHT 7
static const char GRASS_REG_A[]  = "wvwwWWWwwvwwvwWwwwWwwwv";
static const char GRASS_REG_B[]  = "wwvwvwwWWWwwvwWwwWwwwwwvwWwwwWWWWWwwwWWwvwWwwwwwwWWWWWWwwwWWwv";
static const char GRASS_REG_C[]  = "wwvwWwwWwwwvwvwWwwwwWWWwwwwWWwvwWwwwwwWWWWwwwWWwvwWwwwwwwWWWWWwwwWWwv";
static const char GRASS_REG_D[]  = "wwvwvwwWWWwwvwWwwWwwwwwvwWwwwwwWWWWWwwwWWwvwWwwwwwwWWWWWWwwwWWwv";
static const char GRASS_REG_SP[] = "wwvwvwwWWWwwvwWwwwwWwwwwwvwWwwwWWWWWwwwWWwvwWwwwwwwWWWWWWwwwWWwv";
static const char GRASS_REG_BP[] = "wwvwvwwWWWwwvwWwwWwwwwwvwWwwwwwWWWWWwwwWWwvwWwwwwwwWWWWWWwwwWWwvwWwwwwwwwWWWWWWWwwwWWwv";

#define GRASS_CMP_WEIGHT 4
static const char GRASS_CMP_GT[] = "wwvwvwwWWWwwvwWwwwwWwwwwwWwwwwv";
static const char GRASS_CMP_LT[] = "wwvwvwwWWWwwvwWwwwwWwwwWwwwwwwv";
static const char GRASS_CMP_EQ[] = "wvwwWWWwwvwwvwWwwwWwwwWwwwwv";
static const char GRASS_CMP_LE[] = "wvwwWWWwwvwwvwWwwwWwwwwWwwwwv";
static const char GRASS_CMP_GE[] = "wvwwWWWwwvwwvwWwwwWwwwWwwwwwv";
static const char GRASS_CMP_NE[] = "wwvwvwwWWWwwvwWwwwwWwwwWwwwwv";


static int GRASS_BP = 1;

static void grass_apply(int W, int w) {
  for (; W; W--) putchar('W');
  for (; w; w--) putchar('w');
}

static void grass_apply_stack_top_to_grassvm() {
  grass_apply(GRASS_BP, 1);
  putchar('v');
  GRASS_BP = 1;
}

static int emit_grass_reg_helper(const char* s, const int i) {
  fputs(s, stdout);
  GRASS_BP += i;
  return GRASS_BP;
}

static void emit_grass_reg(Reg r) {
  switch (r) {
  case A: emit_grass_reg_helper(GRASS_REG_A, GRASS_REG_A_WEIGHT); return;
  case B: emit_grass_reg_helper(GRASS_REG_B, GRASS_REG_B_WEIGHT); return;
  case C: emit_grass_reg_helper(GRASS_REG_C, GRASS_REG_C_WEIGHT); return;
  case D: emit_grass_reg_helper(GRASS_REG_D, GRASS_REG_D_WEIGHT); return;
  case BP: emit_grass_reg_helper(GRASS_REG_BP, GRASS_REG_BP_WEIGHT); return;
  case SP: emit_grass_reg_helper(GRASS_REG_SP, GRASS_REG_SP_WEIGHT); return;
  default:
    error("unknown register: %d", r);
  }
}

static int emit_grass_t_nil(int t_nil) {
  if (t_nil) {
    // \x.x
    fputs("wv", stdout);
    // \x.\y.x = \x.\y.(3 2)
    fputs("wwWWWwwv", stdout);
    GRASS_BP += 2;
  } else {
    // \x.\y.y
    fputs("wwv", stdout);
    GRASS_BP++;
  }
  return GRASS_BP;
}

static int emit_grass_int_inst(int n) {
#ifndef __eir__
  n &= ((1 << GRASS_N_BITS) - 1);
#endif
  // \x.x
  fputs("wv", stdout);
  // \x.\y.x : Initial index: 2
  fputs("wwWWWwwv", stdout);
  // \x.\y.y : Initial index: 1
  fputs("wwv", stdout);
  GRASS_BP += 3;
  for (int i = 0; i < GRASS_N_BITS; i++) {
    const int checkbit = 1 << i;
    const int t_index = i + 1 + 2;
    const int nil_index = i + 1 + 1;
    putchar('w');
    grass_apply(1, (n & checkbit) ? nil_index : t_index);
    grass_apply(1, 3);
    putchar('v');
    GRASS_BP++;
  }
  return GRASS_BP;
}

static int emit_grass_isimm(Value* v) {
  if (v->type == REG) {
    emit_grass_t_nil(GRASS_NIL);
  } else if (v->type == IMM) {
    emit_grass_t_nil(GRASS_T);
  } else {
    error("invalid value");
  }
  return GRASS_BP;
}

static int emit_grass_value_str(Value* v) {
  if (v->type == REG) {
    emit_grass_reg(v->reg);
  } else if (v->type == IMM) {
    emit_grass_int_inst(v->imm);
  } else {
    error("invalid value");
  }
  return GRASS_BP;
}

static int emit_grass_int_data(int n) {
#ifndef __eir__
  n &= ((1 << GRASS_N_BITS) - 1);
#endif
  // \x.x
  fputs("wv", stdout);
  // \x.\y.y
  fputs("wwv", stdout);
  // \x.\y.x
  fputs("wwWWWWwwv", stdout);
  GRASS_BP += 3;

  // Apply t to initialize data insertion
  grass_apply_stack_top_to_grassvm();

  // Apply integers
  for (int i = 0; i < GRASS_N_BITS; i++) {
    const int checkbit = 1 << (GRASS_N_BITS - 1 - i);
    const int nil_index = i + 3;
    const int t_index = i + 2;
    grass_apply(1, (n & checkbit) ? nil_index : t_index);
  }
  putchar('v');
  GRASS_BP = 1;
  putchar('\n');
  return GRASS_BP;
}

static Data* grass_reverse_data(Data* data) {
  Data* prev = NULL;
  while (data) {
    Data* next = data->next;
    data->next = prev;
    prev = data;
    data = next;
  }
  return prev;
}

static void emit_grass_data_list(Data* data) {
  data = grass_reverse_data(data);
  putchar('\n');
  for (; data; data = data->next){
    emit_grass_int_data(data->v);
  }
  // \x.\y.y
  fputs("wwv", stdout);
  GRASS_BP++;
  // Apply nil to end data insertion
  grass_apply_stack_top_to_grassvm();
  putchar('\n');
}

static int emit_grass_inst_tag(int i_tag) {
  if (i_tag == GRASS_INST_MOV) {
    // \x.\y.\z.\a.\b.\c.\d.\e.e
    fputs("wwwwwwwwv", stdout);
    GRASS_BP++;
  } else {
    // \x.x
    fputs("wv", stdout);
    // (\x.\y.\z.\a.\b.\c.\d.\e. (2 i_tag))
    fputs("wwwwwwww", stdout);
    grass_apply(9, 9 - i_tag);
    putchar('v');
    GRASS_BP += 2;
  }
  return GRASS_BP;
}

static int emit_grass_io_tag(int io_tag) {
  if (io_tag == GRASS_IO_EXIT) {
    // \x.\y.\z.z
    fputs("wwwv", stdout);
    GRASS_BP++;
  } else {
    // \x.x
    fputs("wv", stdout);
    // (\x.\y.\z. (2 io_tag))
    fputs("www", stdout);
    grass_apply(4, 4 - io_tag);
    putchar('v');
    GRASS_BP += 2;
  }
  return GRASS_BP;
}

static int emit_grass_n_tuple(int* cons_bp) {
  putchar('w');
  for (int i = 0; cons_bp[i]; i++) {
    grass_apply(1, i + 1 + GRASS_BP - (cons_bp[i] - 1));
  }
  putchar('v');
  GRASS_BP++;
  return GRASS_BP;
}

static int emit_grass_enum_cmp(const char* enum_cmp) {
  fputs(enum_cmp, stdout);
  GRASS_BP += GRASS_CMP_WEIGHT;
  return GRASS_BP;
}

static void emit_grass_cmp_inst(const char* enum_cmp, Inst* inst, int* inst_cons4) {
  inst_cons4[0] = emit_grass_inst_tag(GRASS_INST_CMP);
  inst_cons4[1] = emit_grass_isimm(&inst->src);
  inst_cons4[2] = emit_grass_value_str(&inst->src);
  int cmp_cons[3];
  cmp_cons[0] = emit_grass_enum_cmp(enum_cmp);
  cmp_cons[1] = emit_grass_value_str(&inst->dst);
  cmp_cons[2] = 0;
  inst_cons4[3] = emit_grass_n_tuple(cmp_cons);
}

static void emit_grass_jmpcmp_inst(const char* enum_cmp, Inst* inst, int* inst_cons4) {
  inst_cons4[0] = emit_grass_inst_tag(GRASS_INST_JMPCMP);
  inst_cons4[1] = emit_grass_isimm(&inst->src);
  inst_cons4[2] = emit_grass_value_str(&inst->src);
  int jmpcmp_cons[5];
  jmpcmp_cons[0] = emit_grass_enum_cmp(enum_cmp);
  jmpcmp_cons[1] = emit_grass_isimm(&inst->jmp);
  jmpcmp_cons[2] = emit_grass_value_str(&inst->jmp);
  jmpcmp_cons[3] = emit_grass_value_str(&inst->dst);
  jmpcmp_cons[4] = 0;
  inst_cons4[3] = emit_grass_n_tuple(jmpcmp_cons);
}

static void emit_grass_basic_inst(int inst_tag, Inst* inst, int* inst_cons4) {
  inst_cons4[0] = emit_grass_inst_tag(inst_tag);
  inst_cons4[1] = emit_grass_isimm(&inst->src);
  inst_cons4[2] = emit_grass_value_str(&inst->src);
  inst_cons4[3] = emit_grass_value_str(&inst->dst);
}

static void emit_grass_inst(Inst* inst) {
  int inst_cons4[5];
  inst_cons4[4] = 0;

  switch (inst->op) {
  case MOV: emit_grass_basic_inst(GRASS_INST_MOV, inst, inst_cons4); break;
  case LOAD: emit_grass_basic_inst(GRASS_INST_LOAD, inst, inst_cons4); break;
  case STORE: emit_grass_basic_inst(GRASS_INST_STORE, inst, inst_cons4); break;

  case EQ: emit_grass_cmp_inst(GRASS_CMP_EQ, inst, inst_cons4); break;
  case NE: emit_grass_cmp_inst(GRASS_CMP_NE, inst, inst_cons4); break;
  case LT: emit_grass_cmp_inst(GRASS_CMP_LT, inst, inst_cons4); break;
  case GT: emit_grass_cmp_inst(GRASS_CMP_GT, inst, inst_cons4); break;
  case LE: emit_grass_cmp_inst(GRASS_CMP_LE, inst, inst_cons4); break;
  case GE: emit_grass_cmp_inst(GRASS_CMP_GE, inst, inst_cons4); break;

  case JEQ: emit_grass_jmpcmp_inst(GRASS_CMP_EQ, inst, inst_cons4); break;
  case JNE: emit_grass_jmpcmp_inst(GRASS_CMP_NE, inst, inst_cons4); break;
  case JLT: emit_grass_jmpcmp_inst(GRASS_CMP_LT, inst, inst_cons4); break;
  case JGT: emit_grass_jmpcmp_inst(GRASS_CMP_GT, inst, inst_cons4); break;
  case JLE: emit_grass_jmpcmp_inst(GRASS_CMP_LE, inst, inst_cons4); break;
  case JGE: emit_grass_jmpcmp_inst(GRASS_CMP_GE, inst, inst_cons4); break;

  case JMP: {
    inst_cons4[0] = emit_grass_inst_tag(GRASS_INST_JMP);
    inst_cons4[1] = emit_grass_isimm(&inst->jmp);
    inst_cons4[2] = emit_grass_value_str(&inst->jmp);
    inst_cons4[3] = emit_grass_t_nil(GRASS_NIL);
    break;
  }

  case ADD: {
    inst_cons4[0] = emit_grass_inst_tag(GRASS_INST_ADDSUB);
    inst_cons4[1] = emit_grass_isimm(&inst->src);
    inst_cons4[2] = emit_grass_value_str(&inst->src);
    int add_cons[3];
    add_cons[0] = emit_grass_value_str(&inst->dst);
    add_cons[1] = emit_grass_t_nil(GRASS_T);
    add_cons[2] = 0;
    inst_cons4[3] = emit_grass_n_tuple(add_cons);
    break;
  }

  case SUB: {
    inst_cons4[0] = emit_grass_inst_tag(GRASS_INST_ADDSUB);
    inst_cons4[1] = emit_grass_isimm(&inst->src);
    inst_cons4[2] = emit_grass_value_str(&inst->src);
    int sub_cons[3];
    sub_cons[0] = emit_grass_value_str(&inst->dst);
    sub_cons[1] = emit_grass_t_nil(GRASS_NIL);
    sub_cons[2] = 0;
    inst_cons4[3] = emit_grass_n_tuple(sub_cons);
    break;
  }

  case PUTC: {
    inst_cons4[0] = emit_grass_inst_tag(GRASS_INST_IO);
    inst_cons4[1] = emit_grass_isimm(&inst->src);
    inst_cons4[2] = emit_grass_value_str(&inst->src);
    inst_cons4[3] = emit_grass_io_tag(GRASS_IO_PUTC);
    break;
  }

  case GETC: {
    inst_cons4[0] = emit_grass_inst_tag(GRASS_INST_IO);
    inst_cons4[1] = emit_grass_t_nil(GRASS_NIL);
    inst_cons4[2] = emit_grass_value_str(&inst->dst);
    inst_cons4[3] = emit_grass_io_tag(GRASS_IO_GETC);
    break;
  }

  case EXIT: {
    inst_cons4[0] = emit_grass_inst_tag(GRASS_INST_IO);
    inst_cons4[1] = emit_grass_t_nil(GRASS_NIL);
    inst_cons4[2] = emit_grass_t_nil(GRASS_NIL);
    inst_cons4[3] = emit_grass_io_tag(GRASS_IO_EXIT);
    break;
  }

  case DUMP: {
    inst_cons4[0] = emit_grass_inst_tag(GRASS_INST_MOV);
    inst_cons4[1] = emit_grass_t_nil(GRASS_NIL);
    inst_cons4[2] = emit_grass_reg_helper(GRASS_REG_A, GRASS_REG_A_WEIGHT);
    inst_cons4[3] = emit_grass_reg_helper(GRASS_REG_A, GRASS_REG_A_WEIGHT);
    break;
  }

  default:
    error("oops");
  }

  emit_grass_n_tuple(inst_cons4);
  grass_apply_stack_top_to_grassvm();
  fputs("\n", stdout);
}

static Inst* grass_reverse_instructions(Inst* inst) {
  Inst* prev = NULL;
  while (inst) {
    Inst* next = inst->next;
    inst->next = prev;
    prev = inst;
    inst = next;
  }
  return prev;
}

static Inst* emit_grass_chunk(Inst* inst) {
  const int init_pc = inst->pc;
  for (; inst && (inst->pc == init_pc); inst = inst->next) {
    emit_grass_inst(inst);
  }
  emit_grass_t_nil(GRASS_NIL);
  grass_apply_stack_top_to_grassvm();
  putchar('\n');
  return inst;
}

static void emit_grass_text_list(Inst* inst) {
  inst = grass_reverse_instructions(inst);
  
  while (inst) {
    inst = emit_grass_chunk(inst);
  }
  emit_grass_t_nil(GRASS_NIL);
  grass_apply_stack_top_to_grassvm();
  putchar('\n');
}

void target_w(Module* module) {
  fputs(GRASS_VM, stdout);
  putchar('v');
  emit_grass_data_list(module->data);
  emit_grass_text_list(module->text);
}
