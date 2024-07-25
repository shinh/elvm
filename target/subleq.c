#include <ir/ir.h>
#include <target/util.h>
#include <stdint.h>
#include <stdlib.h>


#define SUBLEQ_MAX_WORDS_LINE (30)
#define SUBLEQ_ONE_CONST (11)
#define SUBLEQ_NEG_ONE_CONST (10)
#define SUBLEQ_MEM_START_CONST (12)
#define SUBLEQ_JTABLE_START_CONST (13)
#define SUBLEQ_UINTMAX_CONST (14)
#define SUBLEQ_NEG_UINTMAX_CONST (15)
#define SUBLEQ_REG(regnum) (regnum + 3)
#define SUBLEQ_MEM(index) (index + 16)
static const int64_t SUBLEQ_OPS_TABLE[7][3] = {
  {1, 0, 0}, // EQ, JEQ
  {0, 1, 1}, // NE, JNE
  {0, 1, 0}, // LT, JLT
  {0, 0, 1}, // GT, JGT
  {1, 1, 0}, // LE, JLE
  {1, 0, 1}, // GE, JGE
};

typedef struct {
  int64_t word_on;
  int64_t jump_table_start;
  int64_t* pc2addr;
  size_t pc_cnt;
} SubleqGen;

static SubleqGen subleq;

static void subleq_emit_instr(int64_t a, int64_t b, int64_t c){
  emit_line("%d %d %d", a, b, c);
  subleq.word_on += 3;
}

/**
 * Same as *dest -= *src in other languages
*/
static void subleq_emit_sub(int64_t src, int64_t dest){
  subleq.word_on += 3;
  emit_line("%d %d %d", src, dest, subleq.word_on);
}

static void subleq_emit_zero(int64_t loc){
  subleq_emit_sub(loc, loc);
}

/**
 * Same as *dest += *src in other languages
*/
static void subleq_emit_add(int64_t src, int64_t dest){
  subleq_emit_sub(src, 0);
  subleq_emit_sub(0, dest);
  subleq_emit_zero(0);
}

static int64_t subleq_emit_imm(int64_t imm){
  // //emit_line("# Immediate value");
  subleq_emit_instr(0, 0, subleq.word_on + 4);
  emit_line("%d", imm);
  subleq.word_on += 1;
  return subleq.word_on - 1;
}

/**
 * Same as *dest = **src in other languages
*/
static void subleq_emit_load_dblptr(int64_t src, int64_t dest){
  // //emit_line("# Load deref");
  subleq_emit_zero(dest);
  subleq_emit_sub(src, 0);
  subleq_emit_zero(subleq.word_on + 9);
  subleq_emit_sub(0, subleq.word_on + 6);
  subleq_emit_zero(0);
  subleq_emit_sub(0, 0); // First argument gets modified to value stored in *src
  subleq_emit_sub(0, dest);
  subleq_emit_zero(0);
}

/**
 * Same as **dest = *src in other languages
*/
static void subleq_emit_store_dblptr(int64_t src, int64_t dest){
  // //emit_line("# Store deref");
  // Modify 3rd instruction from 0 0 c -> dest dest c
  subleq_emit_zero(subleq.word_on + 25);
  subleq_emit_zero(subleq.word_on + 21);
  subleq_emit_add(dest, subleq.word_on + 18);
  subleq_emit_add(dest, subleq.word_on + 10);
  // //emit_line("# First modified code");
  subleq_emit_sub(0, 0);

  subleq_emit_zero(subleq.word_on + 16);
  subleq_emit_add(dest, subleq.word_on + 13);
  subleq_emit_sub(src, 0);
  // //emit_line("# Second modified code");
  subleq_emit_sub(0, 0);
  subleq_emit_zero(0);
}

// jeq, jlt, and jgt are relative to last line, left is mangled
static void subleq_emit_cmp(int64_t left, int64_t right, int64_t jeq, int64_t jlt, int64_t jgt){
  // //emit_line("# Comparison");
  subleq_emit_sub(right, left);
  subleq_emit_instr(0, left, subleq.word_on + 6);
  subleq_emit_instr(0, 0, subleq.word_on + 6 + jgt);
  subleq_emit_instr(left, 0, subleq.word_on + 3 + jeq);
  subleq_emit_instr(0, 0, subleq.word_on + jlt);
}

static void subleq_emit_wrap_umaxint(int64_t val){
  //emit_line("# Wrapping umaxint");
  subleq_emit_instr(SUBLEQ_NEG_UINTMAX_CONST, val, subleq.word_on);

  subleq_emit_cmp(val, SUBLEQ_UINTMAX_CONST, 6, 3, -12);
  subleq_emit_sub(SUBLEQ_NEG_UINTMAX_CONST, val);
  subleq_emit_zero(0);
}

static void subleq_emit_jump(int64_t jmp_loc){
  subleq_emit_zero(1);

  //emit_line("# Setting PC");
  subleq_emit_zero(SUBLEQ_REG(6));
  subleq_emit_add(jmp_loc, SUBLEQ_REG(6));

  //emit_line("# Storing loc to jump to in 1");
  subleq_emit_add(jmp_loc, 1);
  subleq_emit_add(SUBLEQ_JTABLE_START_CONST, 1);

  //emit_line("# Dereference and store jump table location (negative)");
  subleq_emit_sub(1, 0);
  subleq_emit_zero(subleq.word_on + 9);
  subleq_emit_sub(0, subleq.word_on + 6);
  subleq_emit_zero(0);
  subleq_emit_sub(0, 0);

  //emit_line("# Jump to jump table location");
  subleq_emit_zero(subleq.word_on + 11);
  subleq_emit_sub(0, subleq.word_on + 8);
  subleq_emit_zero(1);
  subleq_emit_instr(0, 0, 0);
}

static void init_state_subleq(Data* data) {

  subleq.word_on = 0;

  // Data length always includes 7 registers and 6 constants
  size_t data_length = 13 + (1 << 24) - 1 + subleq.pc_cnt;

  subleq_emit_instr(0, 0, data_length + 4);
  // Emit registers
  emit_line("0 0 0 0 0 0 0");
  // Emit constants
  emit_line("-1 1 %d %d %d -%d", 
              SUBLEQ_MEM(0), subleq.jump_table_start,
              UINT_MAX + 1, UINT_MAX + 1);

  for (int mp = 0; mp < (1 << 24); mp++) {

    if (mp != 0 && mp % SUBLEQ_MAX_WORDS_LINE == 0){
      emit_line("");
    }

    if (data) {
      emit_str("%d ", data->v);
      data = data->next;

      if (!data){
        emit_line("\n#{loc_skip:%d}", (1 << 24) - mp - 1);
      }

    } else if (((1 << 24) - mp) % SUBLEQ_MAX_WORDS_LINE != 0){
      emit_str("0 ");
    } else {
      emit_str("\n0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0");
      mp += SUBLEQ_MAX_WORDS_LINE - 1;
    }

  }

  subleq.word_on += data_length + 1;
  subleq.jump_table_start = subleq.word_on - subleq.pc_cnt;
  for (size_t addr = 0; addr < subleq.pc_cnt; addr++){
    if (addr % SUBLEQ_MAX_WORDS_LINE == 0)
      emit_line("");
    emit_str("%d ", subleq.pc2addr[addr]);
  }
  emit_line("");

}

static void subleq_emit_inst(Inst* inst) {
  switch (inst->op) {
  case MOV:
    {
      if (inst->src.type != REG || inst->src.reg != inst->dst.reg){
        if (inst->src.type == REG){
          ////emit_line("# Doing move from reg ");
        }
        int64_t src_loc = (inst->src.type == REG) ? (int64_t)SUBLEQ_REG(
          inst->src.reg) : subleq_emit_imm(inst->src.imm);
        subleq_emit_zero(SUBLEQ_REG(inst->dst.reg));
        subleq_emit_add(src_loc, SUBLEQ_REG(inst->dst.reg));
      }
    }
    break;

  case ADD:
    {
      //emit_line("# Doing add");
      int64_t src_loc = (inst->src.type == REG) ? (int64_t)SUBLEQ_REG(
        inst->src.reg) : subleq_emit_imm(inst->src.imm);
      subleq_emit_add(src_loc, SUBLEQ_REG(inst->dst.reg));
      subleq_emit_wrap_umaxint(SUBLEQ_REG(inst->dst.reg));
    }
    break;

  case SUB:
    {
      //emit_line("# Doing sub");
      int64_t src_loc = (inst->src.type == REG) ? (int64_t)SUBLEQ_REG(
        inst->src.reg) : subleq_emit_imm(inst->src.imm);
      subleq_emit_sub(src_loc, SUBLEQ_REG(inst->dst.reg));
      subleq_emit_wrap_umaxint(SUBLEQ_REG(inst->dst.reg));
    }
    break;

  case LOAD:
    if (inst->src.type == IMM){
      //emit_line("# Doing load imm");
      subleq_emit_zero(SUBLEQ_REG(inst->dst.reg));
      subleq_emit_add(SUBLEQ_MEM(inst->src.imm), SUBLEQ_REG(inst->dst.reg));
    } else if (inst->src.type == REG){
      //emit_line("# Doing load reg");
      subleq_emit_add(SUBLEQ_REG(inst->src.reg), 1);
      subleq_emit_add(SUBLEQ_MEM_START_CONST, 1);
      subleq_emit_load_dblptr(1, SUBLEQ_REG(inst->dst.reg));
      subleq_emit_zero(1);
    }
    break;

  case STORE:
    if (inst->src.type == IMM){
      //emit_line("# Doing store imm");
      subleq_emit_zero(SUBLEQ_MEM(inst->src.imm));
      subleq_emit_add(SUBLEQ_REG(inst->dst.reg), SUBLEQ_MEM(inst->src.imm));
    } else if (inst->src.type == REG){
      //emit_line("# Doing store reg dest: %s src: %s", reg_names[inst->src.reg], reg_names[inst->dst.reg]);
      subleq_emit_add(SUBLEQ_REG(inst->src.reg), 1);
      subleq_emit_add(SUBLEQ_MEM_START_CONST, 1);
      subleq_emit_store_dblptr(SUBLEQ_REG(inst->dst.reg), 1);
      subleq_emit_zero(1);
    }
    break;

  case PUTC:
    {
      //emit_line("# Putting char");
      int64_t src_loc = (inst->src.type == REG) ? (int64_t)SUBLEQ_REG(
        inst->src.reg) : subleq_emit_imm(inst->src.imm);
      subleq_emit_instr(src_loc, -1, subleq.word_on + 3);
    }
    break;

  case GETC:
    //emit_line("# Getting char");
    subleq_emit_instr(-1, SUBLEQ_REG(inst->dst.reg), subleq.word_on + 3);
    subleq_emit_cmp(SUBLEQ_REG(inst->dst.reg), SUBLEQ_NEG_ONE_CONST, 6, 3, 3);
    subleq_emit_sub(SUBLEQ_ONE_CONST, SUBLEQ_REG(inst->dst.reg));
    subleq_emit_zero(0);
    break;

  case EXIT:
    // //emit_line("# Exiting");
    subleq_emit_instr(0, 0, -1);
    break;

  case DUMP:
    break;

  case EQ:
  case NE:
  case LT:
  case GT:
  case LE:
  case GE:
    {
      //emit_line("# Doing comparison");
      int64_t src_loc = (inst->src.type == REG) ? (int64_t)SUBLEQ_REG(
        inst->src.reg) : subleq_emit_imm(inst->src.imm);

      subleq_emit_add(SUBLEQ_REG(inst->dst.reg), 1);
      subleq_emit_zero(SUBLEQ_REG(inst->dst.reg));

      int col = inst->op - EQ;
      subleq_emit_cmp(1, src_loc,
        SUBLEQ_OPS_TABLE[col][0] * 3 + 3,
        SUBLEQ_OPS_TABLE[col][1] * 3 + 3,
        SUBLEQ_OPS_TABLE[col][2] * 3 + 3);

      subleq_emit_sub(SUBLEQ_ONE_CONST, SUBLEQ_REG(inst->dst.reg));
      subleq_emit_sub(SUBLEQ_NEG_ONE_CONST, SUBLEQ_REG(inst->dst.reg));
      subleq_emit_zero(1);
    }
    break;

  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
    {
      //emit_line("# Doing cond jump to pc %d", inst->jmp.type == REG ? -1 : inst->jmp.imm);
      int64_t src_loc = (inst->src.type == REG) ? (int64_t)SUBLEQ_REG(
        inst->src.reg) : subleq_emit_imm(inst->src.imm);
      int64_t jmp_loc = (inst->jmp.type == REG) ? (int64_t)SUBLEQ_REG(
        inst->jmp.reg) : subleq_emit_imm(inst->jmp.imm);

      int col = normalize_cond(inst->op, 1) - JEQ;
      subleq_emit_add(SUBLEQ_REG(inst->dst.reg), 1);
      subleq_emit_cmp(1, src_loc,
        SUBLEQ_OPS_TABLE[col][0] * 20 * 3 + 3,
        SUBLEQ_OPS_TABLE[col][1] * 20 * 3 + 3,
        SUBLEQ_OPS_TABLE[col][2] * 20 * 3 + 3);

      subleq_emit_jump(jmp_loc);

      subleq_emit_zero(1);

    }
    break;

    

  case JMP:
    {
      //emit_line("# Doing jump");
      int64_t jmp_loc = (inst->jmp.type == REG) ? (int64_t)SUBLEQ_REG(
        inst->jmp.reg) : subleq_emit_imm(inst->jmp.imm);
      
      subleq_emit_jump(jmp_loc);

    }
    break;
  
  default:
    error("oops");
  }

}

void target_subleq(Module* module) {
  emit_reset();

  subleq.pc_cnt = 0;
  int prev_pc = -1;
  for (Inst* inst = module->text; inst; inst = inst->next) {
    if (prev_pc != inst->pc){
      subleq.pc_cnt++;
      prev_pc = inst->pc;
    }
  }

  subleq.pc2addr = (int64_t*)calloc(subleq.pc_cnt, sizeof(int64_t));
  if (subleq.pc2addr == NULL){
    error("Couldn't allocate jump table for subleq target");
  }

  init_state_subleq(module->data);

  prev_pc = -1;
  for (Inst* inst = module->text; inst; inst = inst->next) {
    if (prev_pc != inst->pc) {
      subleq.pc2addr[inst->pc] = subleq.word_on;
    }
    prev_pc = inst->pc;
    subleq_emit_inst(inst);
  }

  emit_reset();
  emit_start();

  init_state_subleq(module->data);

  prev_pc = -1;
  for (Inst* inst = module->text; inst; inst = inst->next) {
    if (prev_pc != inst->pc) {
      emit_line("\n# PC = %d", inst->pc);
    }
    prev_pc = inst->pc;
    emit_line("");
    subleq_emit_inst(inst);
  }

  free(subleq.pc2addr);

}
