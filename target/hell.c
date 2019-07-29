#include <ir/ir.h>
#include <target/util.h>

// TODO: dramatically reduce size of generated HeLL file s.t. LMFAO will not crash on so many tests

// TODO: speed up EQ and NEQ test by writing own comparison method (instead of calling SUB two times as of now).
// TODO: speed up getc by more efficient testing for C21, C2 (using SUB multiple times as of now is the lazy, but slow way to implement this test)

void target_hell(Module* module); /// generate and print HeLL code for given program

typedef enum {
  REG_A = 0, REG_B = 1, REG_C = 2, REG_D = 3,
  REG_BP = 4, REG_SP = 5,
  ALU_SRC = 6, ALU_DST = 7, TMP = 8, CARRY = 9, VAL_1222 = 10,
  TMP2 = 11, TMP3 = 12
} HellVariable;


static const char* HELL_VARIABLE_NAMES[] = {
  "reg_a", "reg_b", "reg_c", "reg_d",
  "reg_bp", "reg_sp",
  "alu_src", "alu_dst", "tmp", "carry", "val_1222",
  "tmp2", "tmp3", "" // NULL instead of "" crashes 8cc
};
// --> labels: opr_reg_a, rot_reg_a, reg_a; opr_reg_b, ...; .....

static int num_variable_ret_labels[] = {
  0, 0, 0, 0,
  0, 0,
  0, 0, 0, 0, 0,
  0, 0
};

static int num_flags = 0;
static int num_rotwidth_loop_calls = 0;
static int num_local_labels = 0;
static int current_pc_value = -1;


typedef struct {
  const char* function_name;
  const char* flag_name;
} HeLLFunction;


typedef enum {
  HELL_GENERATE_1222=0,
  HELL_ADD=1,
  HELL_SUB=2,
  HELL_OPR_MEMPTR=3,
  HELL_OPR_MEMORY=4,
  HELL_SET_MEMPTR=5,
  HELL_READ_MEMORY=6,
  HELL_WRITE_MEMORY=7,
  HELL_COMPUTE_MEMPTR=8,
  HELL_TEST_LT=9,
  HELL_TEST_GE=10,
  HELL_TEST_EQ=11,
  HELL_TEST_NEQ=12,
  HELL_TEST_GT=13,
  HELL_TEST_LE=14,
  HELL_MODULO=15,
  HELL_SUB_UINT24=16,
  HELL_ADD_UINT24=17,
  HELL_GETC=18,
  HELL_PUTC=19
} HeLLFunctions;

static const HeLLFunction HELL_FUNCTIONS[] = {
  {"generate_1222", "GENERATE_1222_"},
  {"add", "ADD"},
  {"sub", "SUB"},
  {"opr_memptr", "OPR_MEMPTR"},
  {"opr_memory", "OPR_MEMORY"},
  {"set_memptr", "SET_MEMPTR"},
  {"read_memory", "READ_MEMORY"},
  {"write_memory", "WRITE_MEMORY"},
  {"compute_memptr", "COMPUTE_MEMPTR"},
  {"test_lt", "TEST_LT"},
  {"test_ge", "TEST_GE"},
  {"test_eq", "TEST_EQ"},
  {"test_neq", "TEST_NEQ"},
  {"test_gt", "TEST_GT"},
  {"test_le", "TEST_LE"},
  {"modulo", "MODULO"},
  {"sub_uint24", "SUB_UINT24_"},
  {"add_uint24", "ADD_UINT24_"},
  {"getc", "GETC"},
  {"putc", "PUTC"}
};

static int num_calls[] = {
  0, 0, 0, 0, 0,
  0, 0, 0, 0, 0,
  0, 0, 0, 0, 0,
  0, 0, 0, 0, 0,
};



// initialization and finalization (almost finalization)
static void emit_putc_base(); // write a character from ALU_DST to stdout (modulo 256) ;;; changes ALU_DST by mod 256 computation ;;; changes TMP2, TMP3
static void emit_getc_base(); // read a character from stdin into ALU_DST (modulo 256; revert newline to '\n'; convert EOF to '\0') ;;; changes ALU_SRC, TMP2, TMP3
static void emit_add_uint24_base(); // arithmetic: add ALU_SRC to ALU_DST; ALU_SRC gets destroyed! ;;; side effect: changes TMP2, TMP3
static void emit_sub_uint24_base(); // arithmetic: sub ALU_SRC from ALU_DST; ALU_SRC gets destroyed! ;;; side effect: changes TMP2, TMP3
static void emit_modulo_base(); // ALU_DST := ALU_DST % ALU_SRC ;;; side effect: changes TMP2, TMP3
static void emit_test_le_base(); // ALU_DST := (ALU_DST <= ALU_SRC)?1:0 ;;; side effect: changes TMP2
static void emit_test_gt_base(); // ALU_DST := (ALU_DST > ALU_SRC)?1:0  ;;; side effect: changes TMP2
static void emit_test_neq_base(); // ALU_DST := (ALU_DST != ALU_SRC)?1:0
static void emit_test_eq_base(); // ALU_DST := (ALU_DST == ALU_SRC)?1:0
static void emit_test_ge_base(); // ALU_DST := (ALU_DST >= ALU_SRC)?1:0
static void emit_test_lt_base(); // ALU_DST := (ALU_DST < ALU_SRC)?1:0
static void emit_compute_memptr_base(); // ALU_DST := (MEMORY - 2) - 2* ALU_SRC ;;; side effect: changes TMP2
static void emit_write_memory_base(); // copy ALU_DST to memory
static void emit_read_memory_base(); // copy memory to ALU_DST
static void emit_set_memptr_base(); // copy ALU_DST to memptr (without any changes);; to access cell i, memptr must contain "(MEMORY - 2) - 2*i"
static void emit_memory_access_base(); // OPR memory // OPR memptr
static void emit_add_base(); // arithmetic: add ALU_SRC to ALU_DST; ALU_SRC gets destroyed!; tmp_IS_C1-flag: overflow
static void emit_sub_base(); // arithmetic: sub ALU_SRC from ALU_DST; ALU_SRC gets destroyed!; tmp_IS_C1-flag: overflow ;;;; side effect: ALU_DST changes from 0t.. to 1t..
static void emit_generate_val_1222_base(); // set VAL_1222 to 1t22.22
static void emit_rotwidth_loop_base();
static void emit_hell_variables_base(); // declare variables used in HeLL
static void emit_branch_lookup_table(); // lookup-table to perform jmp instruction by address stored in register
static void emit_function_footer(HeLLFunctions hf); // generate function footer
static void finalize_hell();
static void init_state_hell(Data* data);


// use
static void hell_emit_inst(Inst* inst); // produce HeLL code for a given instruction
static void emit_call(HeLLFunctions hf); // call some function
static void emit_jmp(Value* jmp);
static void emit_read_0tvar(int var); // set VAL_1222 to 1t22..22, read a value starting with 0t from var
static void emit_read_1tvar(int var); // set VAL_1222 to 1t22..22, read a value starting with 1t from var
static void emit_clear_var(int var); // set var AND tmp to C1, prepare for writing...
static void emit_write_var(int var); // write to var; tmp and var must be set to C1 before
static void emit_opr_var(int var); // OPR var
static void emit_rot_var(int var); // ROT var
static void emit_test_var(int var); // MOVD var
static void emit_rotwidth_loop_begin(); // for (int i=0; i<rotwidth; i++) {
static void emit_rotwidth_loop_end();   // }


// helper
static void dec_ternary_string(char* str);
static int hell_cmp_call(Inst* inst);
static void emit_indented(const char* fmt, ...);
static void emit_nonindented(const char* fmt, ...);


void emit_nonindented(const char* fmt, ...) {
  if (fmt[0]) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
  }
  putchar('\n');
}

void emit_indented(const char* fmt, ...) {
  if (fmt[0]) {
    putchar(' ');
    putchar(' ');
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
  }
  putchar('\n');
}


static void emit_load_immediate(unsigned int immediate) {
  num_local_labels++;
  emit_nonindented("local_label_%u:",num_local_labels);
  emit_indented("ROT C0 R_ROT");
  emit_opr_var(VAL_1222);
  emit_indented("OPR %u R_OPR", immediate);
  emit_indented("LOOP2 local_label_%u",num_local_labels);
}


static int hell_cmp_call(Inst* inst) {
  int op = normalize_cond(inst->op, 0);
  switch (op) {
    case JEQ:
      return HELL_TEST_EQ;
    case JNE:
      return HELL_TEST_NEQ;
    case JLT:
      return HELL_TEST_LT;
    case JGT:
      return HELL_TEST_GT;
    case JLE:
      return HELL_TEST_LE;
    case JGE:
      return HELL_TEST_GE;
    default:
      error("oops");
  }
}


static void hell_emit_inst(Inst* inst) {
  switch (inst->op) {
    case MOV:

    emit_clear_var(inst->dst.reg);
    if (inst->src.type == REG) {
      emit_read_0tvar(inst->src.reg);
    }else if (inst->src.type == IMM) {
      emit_load_immediate(inst->src.imm);
    }else{
      error("invalid value");
    }
    emit_write_var(inst->dst.reg);
    break;

  case ADD:

    // copy to ALU
    emit_clear_var(ALU_DST);
    emit_read_0tvar(inst->dst.reg);
    emit_write_var(ALU_DST);

    emit_clear_var(ALU_SRC);
    if (inst->src.type == REG) {
      emit_read_0tvar(inst->src.reg);
    }else if (inst->src.type == IMM) {
      emit_load_immediate(inst->src.imm);
    }else{
      error("invalid value");
    }
    emit_write_var(ALU_SRC);

    // compute
    emit_call(HELL_ADD_UINT24);

    // copy result back
    emit_clear_var(inst->dst.reg);
    emit_read_0tvar(ALU_DST);
    emit_write_var(inst->dst.reg);
    break;

  case SUB:

    // copy to ALU
    emit_clear_var(ALU_DST);
    emit_read_0tvar(inst->dst.reg);
    emit_write_var(ALU_DST);

    emit_clear_var(ALU_SRC);
    if (inst->src.type == REG) {
      emit_read_0tvar(inst->src.reg);
    }else if (inst->src.type == IMM) {
      emit_load_immediate(inst->src.imm);
    }else{
      error("invalid value");
    }
    emit_write_var(ALU_SRC);

    // compute
    emit_call(HELL_SUB_UINT24);

    // copy result back
    emit_clear_var(inst->dst.reg);
    emit_read_0tvar(ALU_DST);
    emit_write_var(inst->dst.reg);
    break;

  case LOAD:
    // set memory pointer to address
    emit_clear_var(ALU_SRC);
    if (inst->src.type == REG) {
      emit_read_0tvar(inst->src.reg);
    }else if (inst->src.type == IMM) {
      emit_load_immediate(inst->src.imm);
    }else{
      error("invalid value");
    }
    emit_write_var(ALU_SRC);
    emit_call(HELL_COMPUTE_MEMPTR);
    emit_call(HELL_SET_MEMPTR);

    // load value
    emit_call(HELL_READ_MEMORY);
    emit_clear_var(inst->dst.reg);
    emit_read_0tvar(ALU_DST);
    emit_write_var(inst->dst.reg);
    break;

  case STORE:
    // set memory pointer to address
    emit_clear_var(ALU_SRC);
    if (inst->src.type == REG) {
      emit_read_0tvar(inst->src.reg);
    }else if (inst->src.type == IMM) {
      emit_load_immediate(inst->src.imm);
    }else{
      error("invalid value");
    }
    emit_write_var(ALU_SRC);
    emit_call(HELL_COMPUTE_MEMPTR);
    emit_call(HELL_SET_MEMPTR);

    // store value
    emit_clear_var(ALU_DST);
    emit_read_0tvar(inst->dst.reg);
    emit_write_var(ALU_DST);
    emit_call(HELL_WRITE_MEMORY);
    break;

  case PUTC:

  // copy to ALU
  emit_clear_var(ALU_DST);
    if (inst->src.type == REG) {
      emit_read_0tvar(inst->src.reg);
    }else if (inst->src.type == IMM) {
      emit_load_immediate(inst->src.imm);
    }else{
      error("invalid value");
    }
    emit_write_var(ALU_DST);

    // compute
    emit_call(HELL_PUTC);
    break;

  case GETC:

    // compute
    emit_call(HELL_GETC);

    // copy result back
    emit_clear_var(inst->dst.reg);
    emit_read_0tvar(ALU_DST);
    emit_write_var(inst->dst.reg);
    break;

  case EXIT:
    emit_indented("HALT");
    break;

  case DUMP:
    // emit_indented("NOP"); // necessary? remove this line?
    break;

  case EQ:
  case NE:
  case LT:
  case GT:
  case LE:
  case GE:
    // copy to ALU
    emit_clear_var(ALU_DST);
    emit_read_0tvar(inst->dst.reg);
    emit_write_var(ALU_DST);

  emit_clear_var(ALU_SRC);
    if (inst->src.type == REG) {
      emit_read_0tvar(inst->src.reg);
    }else if (inst->src.type == IMM) {
      emit_load_immediate(inst->src.imm);
    }else{
      error("invalid value");
    }
    emit_write_var(ALU_SRC);

    // compute
    emit_call(hell_cmp_call(inst));

    // copy result back
    emit_clear_var(inst->dst.reg);
    emit_read_0tvar(ALU_DST);
    emit_write_var(inst->dst.reg);
    break;


  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
    // copy to ALU
    emit_clear_var(ALU_DST);
    emit_read_0tvar(inst->dst.reg);
    emit_write_var(ALU_DST);

    emit_clear_var(ALU_SRC);
    if (inst->src.type == REG) {
      emit_read_0tvar(inst->src.reg);
    }else if (inst->src.type == IMM) {
      emit_load_immediate(inst->src.imm);
    }else{
      error("invalid value");
    }
    emit_write_var(ALU_SRC);

    // compute
    emit_call(hell_cmp_call(inst));

    emit_clear_var(CARRY);
    emit_read_0tvar(ALU_DST);
    emit_opr_var(CARRY);
    emit_test_var(CARRY);
    num_local_labels++;
    {
      int dntjmp = num_local_labels;
      emit_indented("%s_IS_C1 dont_jmp_%u", HELL_VARIABLE_NAMES[CARRY], dntjmp);
      emit_jmp(&inst->jmp);
      emit_nonindented("dont_jmp_%u:", dntjmp);
      emit_indented("NOP");
    }
    break;

  case JMP:
    emit_jmp(&inst->jmp);
    break;

  default:
    error("oops");
  }
}

static void emit_jmp(Value* jmp) {

  if (jmp->type == REG) {

    emit_clear_var(ALU_DST);
    // pc_lookup_table
    num_local_labels++;
    emit_nonindented("local_label_%u:",num_local_labels);
    emit_indented("ROT C1 R_ROT");
    emit_opr_var(VAL_1222);
    emit_indented("OPR pc_lookup_table R_OPR");
    emit_indented("LOOP2 local_label_%u",num_local_labels);
    emit_write_var(ALU_DST);

    for (int i=0;i<2;i++) {
      emit_clear_var(ALU_SRC);
      emit_read_0tvar(jmp->reg);
      emit_write_var(ALU_SRC);
      emit_call(HELL_ADD);
    }

    // change 0t.. prefix to 1t..
    emit_clear_var(ALU_SRC);
    emit_read_1tvar(ALU_DST);
    emit_write_var(ALU_SRC);

      // do the jmp
    emit_indented("MOVDMOVD %s-3",HELL_VARIABLE_NAMES[ALU_SRC]);

    }else if (jmp->type == IMM) {
      num_local_labels++;
      emit_indented("R_MOVDMOVD ?- ?- ?- destroy_movd%u destroy_movd%u:",num_local_labels,num_local_labels); // destroy MOVDMOVD
      emit_indented("MOVD label_pc%u", jmp->imm);
    }else{
      error("invalid value");
    }
    emit_nonindented("");

}


static void emit_call(HeLLFunctions hf) {
  num_calls[hf]++;
  emit_indented("R_%s%u",HELL_FUNCTIONS[hf].flag_name,num_calls[hf]);
  emit_indented("MOVD %s",HELL_FUNCTIONS[hf].function_name);

  emit_nonindented("");
  emit_nonindented("%s_ret%u:",HELL_FUNCTIONS[hf].function_name,num_calls[hf]);

}


static void emit_function_footer(HeLLFunctions hf) {
  for (int i=1; i<=num_calls[hf]; i++) {
    emit_indented("%s%u %s_ret%u R_%s%u",HELL_FUNCTIONS[hf].flag_name,i,HELL_FUNCTIONS[hf].function_name,i,HELL_FUNCTIONS[hf].flag_name,i);
  }
  if (!num_calls[hf]) { emit_indented("0"); /* fix LMFAO problem */ }
  emit_nonindented("");
  emit_nonindented(".CODE");
  for (int i=1; i<=num_calls[hf]; i++) {
    emit_nonindented("%s%u:",HELL_FUNCTIONS[hf].flag_name,i);

    emit_indented("Nop/MovD");
    emit_indented("Jmp");

    emit_nonindented("");
  }
}



// set VAL_1222 to 1t22..22, read from var
static void emit_read_0tvar(int var) {
  if (var == TMP || var == CARRY || var == VAL_1222) {
    error("oops");
  }
  num_local_labels++;
  emit_nonindented("local_label_%u:",num_local_labels);
  emit_indented("ROT C0 R_ROT");
  emit_opr_var(VAL_1222);
  emit_opr_var(var);
  emit_indented("LOOP2 local_label_%u",num_local_labels);
}

// set VAL_1222 to 1t22..22, read from var, but with preceeding 1t11..
static void emit_read_1tvar(int var) {
  if (var == TMP || var == CARRY || var == VAL_1222) {
    error("oops");
  }
  num_local_labels++;
  emit_nonindented("local_label_%u:",num_local_labels);
  emit_indented("ROT C1 R_ROT");
  emit_opr_var(VAL_1222);
  emit_opr_var(var);
  emit_indented("LOOP2 local_label_%u",num_local_labels);
}

// set var AND tmp to C1, prepare for writing...
static void emit_clear_var(int var) {
  if (var == TMP || var == VAL_1222) {
    error("oops");
  }
  emit_indented("ROT C1 R_ROT");
  num_local_labels++;
  emit_nonindented("local_label_%u:",num_local_labels);
  emit_opr_var(var);
  emit_indented("LOOP2 local_label_%u",num_local_labels);
  num_local_labels++;
  emit_nonindented("local_label_%u:",num_local_labels);
  emit_opr_var(TMP);
  emit_indented("LOOP2 local_label_%u",num_local_labels);
}

// write to var; tmp and var must be set to C1 before
static void emit_write_var(int var) {
  if (var == TMP || var == CARRY || var == VAL_1222) {
    error("oops");
  }
  emit_opr_var(TMP);
  emit_opr_var(var);
}


static void emit_putc_base() {
  emit_nonindented(".DATA");
  emit_nonindented("putc:");
  emit_indented("R_MOVD");

  // do mod 256 computation
  // write 256 into ALU_SRC
  emit_clear_var(ALU_SRC);
  emit_load_immediate(256);
  emit_write_var(ALU_SRC);
  emit_call(HELL_MODULO);

  emit_read_0tvar(ALU_DST);
  emit_indented("OUT ?- R_OUT");

  emit_function_footer(HELL_PUTC);
}


static void emit_getc_base() {
  emit_nonindented(".DATA");
  emit_nonindented("getc:");
  emit_indented("R_MOVD");

  emit_clear_var(ALU_DST);
  emit_indented("IN ?- R_IN");
  emit_write_var(ALU_DST);

  // save in TMP3
  emit_clear_var(TMP3);
  emit_read_0tvar(ALU_DST);
  emit_write_var(TMP3);

  // detect C21 and C2

  // DESTROY preceeding 2t... by moving the value back from TMP3
  emit_clear_var(ALU_DST);
  emit_read_0tvar(TMP3);
  emit_write_var(ALU_DST);

  // read UINT_MAX = 16777215 into ALU_SRC (which is much larger than largest Unicode code point,
  // but much smaller than modified special Malbolge Unshackled encoding 0t22..21 or 0t22..22
  emit_clear_var(ALU_SRC);
  emit_load_immediate(UINT_MAX);
  emit_write_var(ALU_SRC);

  // test if alu_dst is less than alu_src
  emit_call(HELL_SUB);
  emit_clear_var(ALU_DST);
  emit_indented("ROT C1 R_ROT");
  emit_indented("%s_IS_C1 handle_normal_input_character", HELL_VARIABLE_NAMES[TMP]);

  // handle special input character:
  // only the last trit matters to determine whether newline or EOF has been read
  // restore last trit from TMP3
  emit_clear_var(ALU_DST);
  emit_indented("ROT C0 R_ROT");
  emit_indented("OPR 1t2 R_OPR");
  emit_opr_var(ALU_DST); // set ALU_DST to 0t2
  emit_read_1tvar(TMP3);
  emit_opr_var(ALU_DST);
  // if last trit of input has been '2', ALU_DST will now be '1'
  // if last trit of input has been '1', ALU_DST will now be '2'

  // now compare with 2:
  // read 2 into ALU_SRC
  emit_clear_var(ALU_SRC);
  emit_load_immediate(2);
  emit_write_var(ALU_SRC);

  // test if alu_dst is less than alu_src
  emit_call(HELL_SUB);
  emit_clear_var(ALU_DST);
  emit_indented("ROT C1 R_ROT");
  emit_indented("%s_IS_C1 handle_eof_input", HELL_VARIABLE_NAMES[TMP]);


  // handle newline
  // write '\n' into ALU_DST
  emit_clear_var(ALU_DST);
  emit_load_immediate('\n');
  emit_write_var(ALU_DST);
  emit_indented("MOVD finish_getc");


  // handle eof
  emit_nonindented("handle_eof_input:");
  // read '\0' into ALU_DST
  emit_clear_var(ALU_DST);
  emit_opr_var(ALU_DST);
  emit_indented("MOVD finish_getc");


  // handle normal character
  emit_nonindented("handle_normal_input_character:");
  // restore from TMP3
  emit_clear_var(ALU_DST);
  emit_read_0tvar(TMP3);
  emit_write_var(ALU_DST);
  // do mod 256 computation
  // write 256 into ALU_SRC
  emit_clear_var(ALU_SRC);
  emit_load_immediate(256);
  emit_write_var(ALU_SRC);
  emit_call(HELL_MODULO);
  // done.

  emit_indented("R_MOVD");
  emit_nonindented("finish_getc:");
  emit_indented("R_MOVD");

  emit_function_footer(HELL_GETC);
}



static void emit_sub_uint24_base() {
  emit_nonindented(".DATA");
  emit_nonindented("sub_uint24:");
  emit_indented("R_MOVD");

  // backup ALU_SRC to TMP2
  emit_clear_var(TMP2);
  emit_read_0tvar(ALU_SRC);
  emit_write_var(TMP2);

  // load UINT_MAX+1 into ALU_SRC
  emit_clear_var(ALU_SRC);
  num_local_labels++;
  emit_nonindented("local_label_%u:",num_local_labels);
  emit_indented("ROT C0 R_ROT");
  emit_opr_var(VAL_1222);
  emit_indented("OPR %u+1 R_OPR",UINT_MAX);
  emit_indented("LOOP2 local_label_%u",num_local_labels);
  emit_write_var(ALU_SRC);

  // add UINT_MAX+1 to ALU_DST
  emit_call(HELL_ADD);

  // restore ALU_SRC from TMP2
  emit_clear_var(ALU_SRC);
  emit_read_0tvar(TMP2);
  emit_write_var(ALU_SRC);

  // do actual subtraction
  emit_call(HELL_SUB);

  // compute result modulo (UINT_MAX+1)
  emit_clear_var(ALU_SRC);
  num_local_labels++;
  emit_nonindented("local_label_%u:",num_local_labels);
  emit_indented("ROT C0 R_ROT");
  emit_opr_var(VAL_1222);
  emit_indented("OPR %u+1 R_OPR",UINT_MAX);
  emit_indented("LOOP2 local_label_%u",num_local_labels);
  emit_write_var(ALU_SRC);
  emit_call(HELL_MODULO);

  emit_function_footer(HELL_SUB_UINT24);
}




static void emit_add_uint24_base() {
  emit_nonindented(".DATA");
  emit_nonindented("add_uint24:");
  emit_indented("R_MOVD");

  // normal computation
  emit_call(HELL_ADD);

  // read (UINT_MAX+1) into ALU_SRC
  emit_clear_var(ALU_SRC);
  num_local_labels++;
  emit_nonindented("local_label_%u:",num_local_labels);
  emit_indented("ROT C0 R_ROT");
  emit_opr_var(VAL_1222);
  emit_indented("OPR %u+1 R_OPR",UINT_MAX);
  emit_indented("LOOP2 local_label_%u",num_local_labels);
  emit_write_var(ALU_SRC);

  // now modulo can be applied
  emit_call(HELL_MODULO);

  emit_function_footer(HELL_ADD_UINT24);
}



static void emit_modulo_base() {
  emit_nonindented(".DATA");
  emit_nonindented("modulo:");
  emit_indented("R_MOVD");

  // save modulus
  emit_clear_var(TMP3);
  emit_read_0tvar(ALU_SRC);
  emit_write_var(TMP3);

  emit_nonindented("continue_modulo:");
  // save current remainder
  emit_clear_var(TMP2);
  emit_read_0tvar(ALU_DST);
  emit_write_var(TMP2);
  // subtract modulus
  emit_call(HELL_SUB);
  // restore modulus
  emit_clear_var(ALU_SRC);
  emit_read_0tvar(TMP3);
  emit_write_var(ALU_SRC);
  // test underflow
  emit_indented("R_%s_IS_C1", HELL_VARIABLE_NAMES[TMP]);
  emit_indented("%s_IS_C1 continue_modulo", HELL_VARIABLE_NAMES[TMP]); // try one more

  // restore remainder

  emit_clear_var(ALU_DST);
  emit_read_0tvar(TMP2);
  emit_write_var(ALU_DST);

  emit_function_footer(HELL_MODULO);
}


static void emit_test_le_base() {
  emit_nonindented(".DATA");
  emit_nonindented("test_le:");
  emit_indented("R_MOVD");

  emit_clear_var(TMP2);
  emit_read_0tvar(ALU_DST);
  emit_write_var(TMP2);

  emit_clear_var(ALU_DST);
  emit_read_0tvar(ALU_SRC);
  emit_write_var(ALU_DST);

  emit_clear_var(ALU_SRC);
  emit_read_0tvar(TMP2);
  emit_write_var(ALU_SRC);

  emit_call(HELL_TEST_GE);

  emit_function_footer(HELL_TEST_LE);
}

static void emit_test_gt_base() {
  emit_nonindented(".DATA");
  emit_nonindented("test_gt:");
  emit_indented("R_MOVD");

  emit_clear_var(TMP2);
  emit_read_0tvar(ALU_DST);
  emit_write_var(TMP2);

  emit_clear_var(ALU_DST);
  emit_read_0tvar(ALU_SRC);
  emit_write_var(ALU_DST);

  emit_clear_var(ALU_SRC);
  emit_read_0tvar(TMP2);
  emit_write_var(ALU_SRC);

  emit_call(HELL_TEST_LT);

  emit_function_footer(HELL_TEST_GT);
}


static void emit_test_neq_base() {
  emit_nonindented(".DATA");
  emit_nonindented("test_neq:");
  emit_indented("R_MOVD");

  emit_call(HELL_SUB);
  emit_clear_var(ALU_SRC);
  emit_indented("ROT C1 R_ROT");
  emit_indented("OPR 0t2 R_OPR");
  emit_indented("OPR 1t0 R_OPR");
  emit_opr_var(ALU_SRC);

  emit_call(HELL_SUB);
  emit_clear_var(ALU_DST);
  emit_indented("ROT C1 R_ROT");
  emit_indented("%s_IS_C1 is_eq", HELL_VARIABLE_NAMES[TMP]);
  emit_indented("OPR 0t2 R_OPR");
  emit_indented("OPR 1t0 R_OPR");
  emit_nonindented("is_eq:");
  emit_opr_var(ALU_DST);

  emit_function_footer(HELL_TEST_NEQ);
}


static void emit_test_eq_base() {
  emit_nonindented(".DATA");
  emit_nonindented("test_eq:");
  emit_indented("R_MOVD");

  emit_call(HELL_SUB);
  emit_clear_var(ALU_SRC);
  emit_indented("ROT C1 R_ROT");
  emit_indented("OPR 0t2 R_OPR");
  emit_indented("OPR 1t0 R_OPR");
  emit_opr_var(ALU_SRC);

  emit_call(HELL_SUB);
  emit_clear_var(ALU_DST);
  emit_indented("ROT C1 R_ROT");
  emit_indented("R_%s_IS_C1", HELL_VARIABLE_NAMES[TMP]);
  emit_indented("%s_IS_C1 is_neq", HELL_VARIABLE_NAMES[TMP]);
  emit_indented("OPR 0t2 R_OPR");
  emit_indented("OPR 1t0 R_OPR");
  emit_nonindented("is_neq:");
  emit_opr_var(ALU_DST);

  emit_function_footer(HELL_TEST_EQ);
}


static void emit_test_ge_base() {
  emit_nonindented(".DATA");
  emit_nonindented("test_ge:");
  emit_indented("R_MOVD");

  emit_call(HELL_SUB);
  emit_clear_var(ALU_DST);
  emit_indented("ROT C1 R_ROT");
  emit_indented("%s_IS_C1 is_lt", HELL_VARIABLE_NAMES[TMP]);
  emit_indented("OPR 0t2 R_OPR");
  emit_indented("OPR 1t0 R_OPR");
  emit_nonindented("is_lt:");
  emit_opr_var(ALU_DST);

  emit_function_footer(HELL_TEST_GE);
}


static void emit_test_lt_base() {
  emit_nonindented(".DATA");
  emit_nonindented("test_lt:");
  emit_indented("R_MOVD");

  emit_call(HELL_SUB);
  emit_clear_var(ALU_DST);
  emit_indented("ROT C1 R_ROT");
  emit_indented("R_%s_IS_C1", HELL_VARIABLE_NAMES[TMP]);
  emit_indented("%s_IS_C1 is_ge", HELL_VARIABLE_NAMES[TMP]);
  emit_indented("OPR 0t2 R_OPR");
  emit_indented("OPR 1t0 R_OPR");
  emit_nonindented("is_ge:");
  emit_opr_var(ALU_DST);

  emit_function_footer(HELL_TEST_LT);
}




static void emit_memory_access_base() {
  emit_nonindented(".DATA");
  emit_nonindented("");
  emit_nonindented("opr_memory:");
  emit_indented("U_MOVDOPRMOVD memptr");
  emit_nonindented("opr_memptr:");
  emit_indented("OPR");
  emit_nonindented("memptr:");
  emit_indented("MEMORY_0 - 2");
  emit_indented("R_OPR");
  emit_indented("R_MOVD");
  emit_function_footer(HELL_OPR_MEMPTR);

  emit_nonindented(".DATA");
  emit_nonindented("");
  emit_nonindented("restore_opr_memory:");
  emit_indented("R_MOVD");
  emit_nonindented("restore_opr_memory_no_r_moved:");
  emit_indented("PARTIAL_MOVDOPRMOVD ?-");
  emit_indented("R_MOVDOPRMOVD ?- ?- ?- ?-");
  emit_indented("LOOP4 half_of_restore_opr_memory_done");
  emit_indented("MOVD restore_opr_memory");
  emit_nonindented("");
  emit_nonindented("half_of_restore_opr_memory_done:");
  emit_indented("LOOP2_2 restore_opr_memory_done");
  emit_indented("PARTIAL_MOVDOPRMOVD");
  emit_indented("restore_opr_memory_no_r_moved");
  emit_nonindented("");
  emit_nonindented("restore_opr_memory_done:");
  emit_function_footer(HELL_OPR_MEMORY);

  emit_nonindented(".CODE");
  emit_nonindented("LOOP4:");
  emit_indented("Nop/Nop/Nop/MovD");
  emit_indented("Jmp");
  emit_nonindented("");
  emit_nonindented("LOOP2_2:");
  emit_indented("Nop/MovD");
  emit_indented("Jmp");
  emit_nonindented("");
  emit_nonindented("MOVDOPRMOVD:");
  emit_indented("MovD/Nop/Nop/Nop/Nop/Nop/Nop/Nop/Nop");
  emit_indented("RNop");
  emit_indented("RNop");
  emit_indented("Opr/Nop/Nop/Nop/Nop/Rot/Nop/Nop/Nop");
  emit_nonindented("PARTIAL_MOVDOPRMOVD:");
  emit_indented("MovD/Nop/Nop/Nop/Nop/Nop/Nop/Nop/Nop");
  emit_indented("Jmp");
  emit_nonindented("");
}


static void emit_compute_memptr_base() {
  ; // ALU_DST := (MEMORY_0 - 2) - 2* ALU_SRC
  emit_nonindented(".DATA");
  emit_nonindented("compute_memptr:");
  emit_indented("R_MOVD");
  // set ALU_DST to MEMORY_0-2
  emit_clear_var(ALU_DST);
  num_local_labels++;
  emit_nonindented("local_label_%u:",num_local_labels);
  emit_indented("ROT C1 R_ROT");
  emit_opr_var(VAL_1222);
  emit_indented("OPR MEMORY_0-2 R_OPR");
  emit_indented("LOOP2 local_label_%u",num_local_labels);
  emit_write_var(ALU_DST);

  // save ALU_SRC to TMP2
  emit_clear_var(TMP2);
  emit_read_0tvar(ALU_SRC);
  emit_write_var(TMP2);

  // sub ALU_SRC from ALU_DST
  emit_call(HELL_SUB);

  // restore ALU_SRC
  emit_clear_var(ALU_SRC);
  emit_read_0tvar(TMP2);
  emit_write_var(ALU_SRC);

  // sub ALU_SRC from ALU_DST
  emit_call(HELL_SUB);

  emit_function_footer(HELL_COMPUTE_MEMPTR);
}


static void emit_write_memory_base() {
  emit_nonindented(".DATA");
  emit_nonindented("write_memory:");
  emit_indented("R_MOVD");

  // add offset (such that uninitialized cells, which are initially 0t10000, equal 0t000)
  // read 0t10000 = 81 into ALU_SRC
  emit_clear_var(ALU_SRC);
  emit_load_immediate(81);
  emit_write_var(ALU_SRC);

  emit_call(HELL_ADD);

  // prepare memory for write (clean cell)
  emit_indented("ROT C1 R_ROT");
  emit_call(HELL_OPR_MEMORY);
  emit_call(HELL_OPR_MEMORY);
  emit_opr_var(TMP);
  emit_opr_var(TMP);

  // read value
  emit_read_0tvar(ALU_DST);

  // store value
  emit_opr_var(TMP);
  emit_call(HELL_OPR_MEMORY);
  emit_function_footer(HELL_WRITE_MEMORY);
}


static void emit_read_memory_base() {
  emit_nonindented(".DATA");
  emit_nonindented("read_memory:");
  emit_indented("R_MOVD");
  emit_clear_var(ALU_DST);

  num_local_labels++;
  emit_nonindented("local_label_%u:",num_local_labels);
  emit_indented("ROT C0 R_ROT");
  emit_opr_var(VAL_1222);
  emit_call(HELL_OPR_MEMORY);
  emit_indented("LOOP2 local_label_%u",num_local_labels);

  emit_write_var(ALU_DST);

  // make backup copy
  emit_clear_var(TMP2);
  emit_read_0tvar(ALU_DST);
  emit_write_var(TMP2);

  // subtract offset (such that uninitialized cells, which are initially 0t10000, equal 0t000)
  // read 0t10000 = 81 into ALU_SRC
  emit_clear_var(ALU_SRC);
  emit_load_immediate(81);
  emit_write_var(ALU_SRC);

  emit_call(HELL_SUB);
  emit_function_footer(HELL_READ_MEMORY);
}


static void emit_set_memptr_base() {
  emit_nonindented(".DATA");
  emit_nonindented("set_memptr:");
  emit_indented("R_MOVD");
  emit_indented("ROT C1 R_ROT");
  emit_call(HELL_OPR_MEMPTR);
  emit_call(HELL_OPR_MEMPTR);
  emit_opr_var(TMP);
  emit_opr_var(TMP);
  emit_read_1tvar(ALU_DST);
  emit_opr_var(TMP);
  emit_call(HELL_OPR_MEMPTR);
  emit_function_footer(HELL_SET_MEMPTR);
}



static void emit_add_base() {

  emit_nonindented(".DATA");
  emit_nonindented("add:");

  emit_indented("%s_IS_C1 add",HELL_VARIABLE_NAMES[TMP]); // set CARRY flag
  emit_indented("R_%s_IS_C1",HELL_VARIABLE_NAMES[TMP]); // clear CARRY flag
  emit_indented("R_MOVD");
  emit_rotwidth_loop_begin();
  // LOOP:
  // if CARRY
  emit_indented("R_%s_IS_C1",HELL_VARIABLE_NAMES[TMP]);
  emit_indented("%s_IS_C1 no_increment_during_add",HELL_VARIABLE_NAMES[TMP]);
  //  unset CARRY and increment ALU_DST
  emit_indented("R_%s_IS_C1",HELL_VARIABLE_NAMES[TMP]);
  emit_nonindented("force_increment_during_add:");
  emit_clear_var(CARRY);
  emit_opr_var(TMP); // set tmp to C0
  emit_opr_var(VAL_1222); // load 1t22..22
  emit_opr_var(CARRY); // set carry to 0t22..22
  emit_indented("ROT C1 R_ROT");
  emit_indented("OPR 0t2 R_OPR");
  emit_opr_var(CARRY); // set carry to 0t22..21
  emit_indented("ROT C1 R_ROT");
  emit_opr_var(VAL_1222); // load 0t22..22
  num_local_labels++;
  emit_nonindented("local_label_%u:",num_local_labels);
  emit_opr_var(ALU_DST); // opr dest
  emit_opr_var(TMP); // opr tmp
  emit_opr_var(CARRY); // opr carry
  emit_indented("LOOP2 local_label_%u",num_local_labels);
  num_local_labels++;
  emit_nonindented("local_label_%u:",num_local_labels);
  emit_indented("ROT C1 R_ROT");
  emit_indented("OPR 0t2 R_OPR"); // load 0t2
  emit_opr_var(TMP); // opr tmp
  emit_indented("LOOP2 local_label_%u",num_local_labels);
  emit_indented("%s_IS_C1 keep_carry_during_add",HELL_VARIABLE_NAMES[TMP]); // keep increment carry-flag
  emit_test_var(TMP); // MOVD tmp
  emit_indented("R_%s_IS_C1",HELL_VARIABLE_NAMES[TMP]);
  emit_nonindented("keep_carry_during_add:");
  emit_indented("R_%s_IS_C1",HELL_VARIABLE_NAMES[TMP]);
  emit_nonindented("no_increment_during_add:");

  // decrement ALU_SRC
  emit_clear_var(CARRY);
  emit_opr_var(TMP); // set tmp to C0
  emit_opr_var(VAL_1222); // load 1t22..22
  emit_opr_var(CARRY); // set carry to 0t22..22
  emit_indented("ROT C1 R_ROT");
  emit_indented("OPR 0t2 R_OPR");
  emit_opr_var(CARRY); // set carry to 1t22..21
  emit_indented("ROT C1 R_ROT");
  emit_opr_var(VAL_1222); // load 0t22..22
  emit_opr_var(ALU_SRC); // OPR src
  emit_indented("ROT C1 R_ROT");
  emit_opr_var(VAL_1222); // load 0t22..22
  emit_opr_var(ALU_SRC); //   OPR src
  emit_opr_var(TMP); //   OPR tmp
  emit_opr_var(CARRY); //   OPR carry
  emit_opr_var(ALU_SRC); //   OPR src
  emit_indented("ROT C1 R_ROT");
  emit_opr_var(VAL_1222); // load 0t22..22
  emit_opr_var(ALU_SRC); //   OPR src
  emit_indented("ROT C1 R_ROT");
  emit_opr_var(VAL_1222); // load 0t22..22
  emit_opr_var(CARRY); //   OPR carry
  emit_indented("ROT C1 R_ROT");
  emit_indented("OPR 0t2 R_OPR"); // load 0t2
  emit_opr_var(CARRY); //   OPR carry
  emit_test_var(CARRY); // MOVD carry
  // IF NO BORROW OCURRED: increment ALU_DST
  emit_indented("%s_IS_C1 force_increment_during_add",HELL_VARIABLE_NAMES[CARRY]); //  GOTO force_increment if CARRY flag is set (=NO BORROW)

  emit_rot_var(ALU_DST); // rot dest
  emit_rot_var(ALU_SRC); // rot src
  emit_rotwidth_loop_end();
  // emit_indented("ROT C0 R_ROT");
  // emit_opr_var(VAL_1222); // restore VAL_1222 to 1t22..22 (instead of 0t22..22)

  emit_function_footer(HELL_ADD);
}



static void emit_sub_base() {
// if tmp_IS_C1-flag is set:
//  overflow occured. -> calling method should handle this
//  possible overflow handling: undo increment, increase rot-width, try increment again

  emit_nonindented(".DATA");
  emit_nonindented("sub:");

  emit_indented("%s_IS_C1 sub",HELL_VARIABLE_NAMES[TMP]); // set CARRY flag
  emit_indented("R_%s_IS_C1",HELL_VARIABLE_NAMES[TMP]); // clear CARRY flag
  emit_indented("R_MOVD");
  emit_rotwidth_loop_begin();
  // LOOP:
  // if CARRY
  emit_indented("R_%s_IS_C1",HELL_VARIABLE_NAMES[TMP]);
  emit_indented("%s_IS_C1 no_decrement_during_sub",HELL_VARIABLE_NAMES[TMP]);
  //  unset CARRY and increment ALU_DST
  emit_indented("R_%s_IS_C1",HELL_VARIABLE_NAMES[TMP]);
  emit_nonindented("force_decrement_during_sub:");

  // decrement ALU_DST
  emit_clear_var(CARRY);
  emit_opr_var(TMP); // set tmp to C0
  emit_opr_var(VAL_1222); // load 1t22..22
  emit_opr_var(CARRY); // set carry to 0t22..22
  emit_indented("ROT C1 R_ROT");
  emit_indented("OPR 0t2 R_OPR");
  emit_opr_var(CARRY); // set carry to 1t22..21
  emit_indented("ROT C1 R_ROT");
  emit_opr_var(VAL_1222); // load 0t22..22
  emit_opr_var(ALU_DST); // OPR src
  emit_indented("ROT C1 R_ROT");
  emit_opr_var(VAL_1222); // load 0t22..22
  emit_opr_var(ALU_DST); //   OPR src
  emit_opr_var(TMP); //   OPR tmp
  emit_opr_var(CARRY); //   OPR carry
  emit_opr_var(ALU_DST); //   OPR src
  emit_indented("ROT C1 R_ROT");
  emit_opr_var(VAL_1222); // load 0t22..22
  emit_opr_var(ALU_DST); //   OPR src
  emit_indented("ROT C1 R_ROT");
  emit_opr_var(VAL_1222); // load 0t22..22
  emit_opr_var(CARRY); //   OPR carry
  emit_indented("ROT C1 R_ROT");
  emit_indented("OPR 0t2 R_OPR"); // load 0t2
  emit_opr_var(CARRY); //   OPR carry
  emit_test_var(CARRY); // MOVD carry

  emit_indented("%s_IS_C1 keep_carry_during_sub",HELL_VARIABLE_NAMES[TMP]); // keep increment carry-flag
  emit_test_var(CARRY); // MOVD tmp
  emit_indented("%s_IS_C1 keep_carry_during_sub",HELL_VARIABLE_NAMES[CARRY]); // if CARRY_IS_C1 is set, TMP_IS_C1 should remain disabled
  emit_indented("R_%s_IS_C1",HELL_VARIABLE_NAMES[TMP]);
  emit_nonindented("keep_carry_during_sub:");
  emit_indented("R_%s_IS_C1",HELL_VARIABLE_NAMES[TMP]);
  emit_nonindented("no_decrement_during_sub:");

  // decrement ALU_SRC
  emit_clear_var(CARRY);
  emit_opr_var(TMP); // set tmp to C0
  emit_opr_var(VAL_1222); // load 1t22..22
  emit_opr_var(CARRY); // set carry to 0t22..22
  emit_indented("ROT C1 R_ROT");
  emit_indented("OPR 0t2 R_OPR");
  emit_opr_var(CARRY); // set carry to 1t22..21
  emit_indented("ROT C1 R_ROT");
  emit_opr_var(VAL_1222); // load 0t22..22
  emit_opr_var(ALU_SRC); // OPR src
  emit_indented("ROT C1 R_ROT");
  emit_opr_var(VAL_1222); // load 0t22..22
  emit_opr_var(ALU_SRC); //   OPR src
  emit_opr_var(TMP); //   OPR tmp
  emit_opr_var(CARRY); //   OPR carry
  emit_opr_var(ALU_SRC); //   OPR src
  emit_indented("ROT C1 R_ROT");
  emit_opr_var(VAL_1222); // load 0t22..22
  emit_opr_var(ALU_SRC); //   OPR src
  emit_indented("ROT C1 R_ROT");
  emit_opr_var(VAL_1222); // load 0t22..22
  emit_opr_var(CARRY); //   OPR carry
  emit_indented("ROT C1 R_ROT");
  emit_indented("OPR 0t2 R_OPR"); // load 0t2
  emit_opr_var(CARRY); //   OPR carry
  emit_test_var(CARRY); // MOVD carry
  // IF NO BORROW OCURRED: increment ALU_DST
  emit_indented("%s_IS_C1 force_decrement_during_sub",HELL_VARIABLE_NAMES[CARRY]); //  GOTO force_increment if CARRY flag is set (=NO BORROW)

  emit_rot_var(ALU_DST); // rot dest
  emit_rot_var(ALU_SRC); // rot src
  emit_rotwidth_loop_end();
  // emit_indented("ROT C0 R_ROT");
  // emit_opr_var(VAL_1222); // restore VAL_1222 to 1t22..22 (instead of 0t22..22)

  emit_function_footer(HELL_SUB);
}



static void emit_generate_val_1222_base() {
  emit_nonindented(".DATA");
  emit_nonindented("generate_1222:");

  emit_indented("R_MOVD");
  emit_indented("ROT C1 R_ROT");
  emit_nonindented("generate_1222_2loop:");
  emit_opr_var(VAL_1222);
  emit_indented("LOOP2 generate_1222_2loop");
  emit_rotwidth_loop_begin();
  emit_indented("ROT C1 R_ROT");
  emit_indented("OPR 0t2 R_OPR");
  emit_opr_var(VAL_1222);
  emit_rot_var(VAL_1222); ////
  emit_rotwidth_loop_end();

  emit_function_footer(HELL_GENERATE_1222);
}



static void emit_opr_var(int var) {
  num_variable_ret_labels[var]++;
  emit_indented("R_%s_RETURN%u",HELL_VARIABLE_NAMES[var],num_variable_ret_labels[var]);
  emit_indented("MOVD opr_%s",HELL_VARIABLE_NAMES[var]);
  emit_nonindented("");
  emit_nonindented("%s_ret%u:",HELL_VARIABLE_NAMES[var],num_variable_ret_labels[var]);
}

static void emit_rot_var(int var) {
  num_variable_ret_labels[var]++;
  emit_indented("R_%s_RETURN%u",HELL_VARIABLE_NAMES[var],num_variable_ret_labels[var]);
  emit_indented("MOVD rot_%s",HELL_VARIABLE_NAMES[var]);
  emit_nonindented("");
  emit_nonindented("%s_ret%u:",HELL_VARIABLE_NAMES[var],num_variable_ret_labels[var]);
}

static void emit_test_var(int var) {
  num_variable_ret_labels[var]++;
  emit_indented("R_%s_RETURN%u",HELL_VARIABLE_NAMES[var],num_variable_ret_labels[var]);
  emit_indented("MOVD %s",HELL_VARIABLE_NAMES[var]);
  emit_nonindented("");
  emit_nonindented("%s_ret%u:",HELL_VARIABLE_NAMES[var],num_variable_ret_labels[var]);
}



static void emit_rotwidth_loop_begin() {
  num_rotwidth_loop_calls++;
  emit_indented("R_ROTWIDTH_LOOP%u",num_rotwidth_loop_calls);
  emit_indented("MOVD loop");

  emit_nonindented("");
  emit_nonindented("rotwidth_loop_inner%u:",num_rotwidth_loop_calls);

  emit_indented("R_ROTWIDTH_LOOP%u",num_rotwidth_loop_calls);
}
static void emit_rotwidth_loop_end() {
  emit_indented("MOVD end_of_loop_body");

  emit_nonindented("");
  emit_nonindented("rotwidth_loop_ret%u:",num_rotwidth_loop_calls);

}

static void emit_rotwidth_loop_base() {
  /** loop over rotwidth */
  int flag_base = num_flags;
  num_flags += 4;

  emit_nonindented(".DATA");
  emit_nonindented("opr_loop_tmp:");

  emit_indented("R_MOVD");
  emit_indented("OPR");

  emit_nonindented("loop_tmp:");

  emit_indented("?");
  emit_indented("U_NOP no_decision");
  emit_indented("U_NOP was_C1");
  emit_indented("U_NOP was_C10");

  emit_nonindented("was_C1:");

  emit_indented("R_LOOP2");
  emit_indented("MOVD leave_loop");

  emit_nonindented("was_C10:");

  emit_indented("R_LOOP2");
  emit_indented("MOVD loop");

  emit_nonindented("no_decision:");

  emit_indented("R_OPR");
  for (int i=1;i<=4;i++) {
    emit_indented("FLAG%u loop_tmp_ret%u R_FLAG%u",flag_base+i,i,flag_base+i);
  }

  emit_nonindented("");
  emit_nonindented("loop:");

  emit_indented("R_MOVD");
  for (int i=1;i<=num_rotwidth_loop_calls;i++){
    emit_indented("ROTWIDTH_LOOP%u rotwidth_loop_inner%u R_ROTWIDTH_LOOP%u",i,i,i);
  }
  // emit_indented("MOVD end_of_loop_body");
  emit_nonindented("");

  emit_nonindented("end_of_loop_body:");

  emit_indented("R_MOVD");
  emit_indented("ROT C1 R_ROT");

  emit_nonindented("reset_loop_tmp_loop:");

  emit_indented("R_FLAG%u",flag_base+1);
  emit_indented("MOVD opr_loop_tmp");
  emit_nonindented("");

  emit_nonindented("loop_tmp_ret1:");

  emit_indented("LOOP2 reset_loop_tmp_loop");

  emit_nonindented("do_twice:");

  emit_indented("ROT C1 R_ROT");
  emit_indented("OPR C02 R_OPR");
  emit_indented("R_FLAG%u",flag_base+2);
  emit_indented("MOVD opr_loop_tmp");
  emit_nonindented("");

  emit_nonindented("loop_tmp_ret2:");

  emit_indented("R_LOOP2");
  emit_indented("LOOP2 loop_tmp");
  emit_indented("R_FLAG%u",flag_base+3);
  emit_indented("MOVD opr_loop_tmp");
  emit_nonindented("");

  emit_nonindented("loop_tmp_ret3:");

  emit_indented("ROT C12 R_ROT");
  emit_indented("R_FLAG%u",flag_base+4);
  emit_indented("MOVD opr_loop_tmp");
  emit_nonindented("");

  emit_nonindented("loop_tmp_ret4:");

  emit_indented("LOOP2 do_twice");
  emit_nonindented("");

  emit_nonindented("leave_loop:");

  emit_indented("R_MOVD");
  for (int i=1;i<=num_rotwidth_loop_calls;i++){
    emit_indented("ROTWIDTH_LOOP%u rotwidth_loop_ret%u R_ROTWIDTH_LOOP%u",i,i,i);
  }

  emit_nonindented("");

  emit_nonindented(".CODE");
  for (int i=1;i<=num_rotwidth_loop_calls;i++){
    emit_nonindented("ROTWIDTH_LOOP%u:",i);

    emit_indented("Nop/MovD");
    emit_indented("Jmp");

    emit_nonindented("");
  }
  emit_nonindented("");

/** END: LOOP over rotwidth */
}


static void emit_hell_variables_base() {
  emit_nonindented(".DATA");
  emit_nonindented("");
  for (int i=0; HELL_VARIABLE_NAMES[i][0] != 0; i++) {
    if (!num_variable_ret_labels[i]) {
      // variable is not USED
      continue;
    }
    emit_nonindented("opr_%s:",HELL_VARIABLE_NAMES[i]);

    emit_indented("R_ROT");
    emit_indented("U_OPR %s",HELL_VARIABLE_NAMES[i]);

    emit_nonindented("rot_%s:",HELL_VARIABLE_NAMES[i]);

    emit_indented("R_OPR");
    emit_indented("U_ROT %s",HELL_VARIABLE_NAMES[i]);

    emit_nonindented("%s:",HELL_VARIABLE_NAMES[i]);

    emit_indented("?");
    emit_indented("U_NOP continue_%s",HELL_VARIABLE_NAMES[i]);
    emit_indented("U_NOP %s_was_c1",HELL_VARIABLE_NAMES[i]);
    emit_indented("U_NOP %s_was_c0",HELL_VARIABLE_NAMES[i]);

    emit_nonindented("%s_was_c1:",HELL_VARIABLE_NAMES[i]);

    emit_indented("%s_IS_C1 %s_was_c1",HELL_VARIABLE_NAMES[i],HELL_VARIABLE_NAMES[i]);
    emit_indented("U_NOP return_from_%s",HELL_VARIABLE_NAMES[i]);

    emit_nonindented("%s_was_c0:",HELL_VARIABLE_NAMES[i]);

    emit_indented("%s_IS_C1 %s_was_c0",HELL_VARIABLE_NAMES[i],HELL_VARIABLE_NAMES[i]);
    emit_indented("R_%s_IS_C1",HELL_VARIABLE_NAMES[i]);
    emit_indented("U_NOP return_from_%s",HELL_VARIABLE_NAMES[i]);

    emit_nonindented("continue_%s:",HELL_VARIABLE_NAMES[i]);

    emit_indented("R_ROT R_OPR");

    emit_nonindented("return_from_%s:",HELL_VARIABLE_NAMES[i]);

    emit_indented("R_MOVD");
    for (int j=1;j<=num_variable_ret_labels[i];j++) {
      emit_indented("%s_RETURN%u %s_ret%u R_%s_RETURN%u",HELL_VARIABLE_NAMES[i],j,HELL_VARIABLE_NAMES[i],j,HELL_VARIABLE_NAMES[i],j);
    }

    emit_nonindented("");
  }

  emit_nonindented(".CODE");
  emit_nonindented("");

  for (int i=0; HELL_VARIABLE_NAMES[i][0] != 0; i++) {
    if (!num_variable_ret_labels[i]) {
      // variable is not USED
      continue;
    }
    emit_nonindented("%s_IS_C1:",HELL_VARIABLE_NAMES[i]);

    emit_indented("Nop/MovD");
    emit_indented("Jmp");

    emit_nonindented("");
    for (int j=1;j<=num_variable_ret_labels[i];j++) {
      emit_nonindented("%s_RETURN%u:",HELL_VARIABLE_NAMES[i],j);

      emit_indented("Nop/MovD");
      emit_indented("Jmp");

      emit_nonindented("");
    }
  }
}

static void emit_branch_lookup_table() {
  emit_nonindented(".DATA");
  emit_nonindented("");
  emit_nonindented("pc_lookup_table:");
  for (int i=0;i<=current_pc_value;i++) {
    emit_indented("MOVD label_pc%u",i);
  }
  emit_nonindented("");
}

static void finalize_hell() {

  emit_putc_base();
  emit_getc_base();
  emit_add_uint24_base();
  emit_sub_uint24_base();
  emit_modulo_base();
  emit_test_le_base();
  emit_test_gt_base();
  emit_test_neq_base();
  emit_test_eq_base();
  emit_test_ge_base();
  emit_test_lt_base();
  emit_compute_memptr_base();
  emit_write_memory_base();
  emit_read_memory_base();
  emit_set_memptr_base();
  emit_memory_access_base();
  emit_add_base();
  emit_sub_base();
  emit_generate_val_1222_base();
  emit_rotwidth_loop_base();
  emit_hell_variables_base();
  emit_branch_lookup_table();

  emit_nonindented(".CODE");
  emit_nonindented("");

  emit_nonindented("MOVD:");

  emit_indented("MovD/Nop");
  emit_indented("Jmp");

  emit_nonindented("");

  emit_nonindented("ROT:");

  emit_indented("Rot/Nop");
  emit_indented("Jmp");

  emit_nonindented("");

  emit_nonindented("OPR:");

  emit_indented("Opr/Nop");
  emit_indented("Jmp");

  emit_nonindented("");

  emit_nonindented("IN:");

  emit_indented("In/Nop");
  emit_indented("Jmp");

  emit_nonindented("");

  emit_nonindented("OUT:");

  emit_indented("Out/Nop");
  emit_indented("Jmp");

  emit_nonindented("");

  emit_nonindented("NOP:");

  emit_indented("Jmp");

  emit_nonindented("");

  emit_nonindented("HALT:");

  emit_indented("Hlt");

  emit_nonindented("");

  emit_nonindented("MOVDMOVD:");

  emit_indented("MovD/Nop");
  emit_indented("RNop");
  emit_indented("RNop");
  emit_indented("RNop");
  emit_indented("MovD/Nop");
  emit_indented("Jmp");

  emit_nonindented("");

  emit_nonindented("LOOP2:");

  emit_indented("MovD/Nop");
  emit_indented("Jmp");

  emit_nonindented("");

  for (int i=1; i<=num_flags; i++) {
    emit_nonindented("FLAG%u:",i);

    emit_indented("Nop/MovD");
    emit_indented("Jmp");

    emit_nonindented("");
  }
}

static void dec_ternary_string(char* str) {
  size_t l = 0;
  while (str[l]) l++;
  while (l) {
    l--;
    str[l]--;
    if (str[l] < '0') {
      str[l] = '2';
    }else{
      break;
    }
  }
}

static void init_state_hell(Data* data) {
  emit_nonindented(".DATA");
  emit_nonindented("");
  emit_nonindented("// backjump");
  emit_nonindented("@1t01112 return_from_memory_cell:");
  emit_indented("R_MOVD");
  emit_indented("MOVD restore_opr_memory");
  emit_nonindented("");
  emit_nonindented("// memory array");

  char address[20];
  for (int i=0; i<13; i++) {
    address[i] = '1';
  }
  address[13] = '0';
  address[14] = '2';
  address[15] = '2';
  address[16] = '2';
  address[17] = '2';
  address[18] = '2';
  address[19] = 0;


  int mp = 0;
  for (; data; data = data->next, mp++) {
    emit_nonindented("@1t%s",address);
    emit_nonindented("MEMORY_%u:", mp);
    if (data->v) {
      emit_indented("%u return_from_memory_cell", data->v + 81); // add offset to fix uninitialized cells to 0
    }else{
      emit_indented("? return_from_memory_cell");
    }
    emit_nonindented("");
    dec_ternary_string(address); dec_ternary_string(address);
  }
  if (!mp) {
    emit_nonindented("@1t022222");
    emit_nonindented("MEMORY_0:");
    emit_indented("? return_from_memory_cell");
    emit_nonindented("");
  }


  emit_nonindented("unused:");
  // force rotation width to be large enough; not increase during program execution...
  // TODO: necessary?? value too small? too large? which value is optimal?
  emit_indented("%u*%u*27 // force rot_width to be large enough", UINT_MAX);
  emit_nonindented("");


  if (data) {
  }
}


void target_hell(Module* module) {
  init_state_hell(module->data);

  if (module->text) { }

  emit_nonindented(".DATA");
  emit_nonindented("ENTRY:");

  emit_call(HELL_GENERATE_1222);




  Inst* inst = module->text;
  for (; inst; inst = inst->next) {
    if (current_pc_value != inst->pc) {
      current_pc_value = inst->pc;
      emit_nonindented("prepare_label_pc%u:",current_pc_value); // to fix issue with unused code
      emit_indented("R_MOVD");
      emit_indented("R_MOVDMOVD ?- ?- ?- label_pc%u",current_pc_value);
      emit_nonindented("label_pc%u:",current_pc_value);
      emit_indented("R_MOVD");
      emit_indented("R_MOVDMOVD ?- ?- ?- ?-");
    }
    hell_emit_inst(inst);
  }
  emit_nonindented("end:"); // fix LMFAO problem
  emit_indented("HALT");

  emit_nonindented("");

  finalize_hell();
}
