#include <ir/ir.h>
#include <target/util.h>

// TODO: build inc/dec/sub/add mod-INT24 functions, PC-logic, and put all together

// minor TODO: fix indention (this file (space vs tab) and HeLL file as well: add more inc_indent,dec_indent)
//             (maybe) remove all these dirty "fix LMFAO problem"-fixes


// TODO: speed up EQ and NEQ test by writing own comparison method (instead of calling SUB two times as of now).


typedef enum {
  REG_A = 0, REG_B = 1, REG_C = 2, REG_D = 3,
  REG_BP = 4, REG_SP = 5, SRC = 6, DST = 7,
  ALU_SRC = 8, ALU_DST = 9, TMP = 10, CARRY = 11, VAL_1222 = 12,
  TMP2 = 13, TMP3 = 14
} HellVariable;

#define GENERATE_1222_ONLY_ONCE 1

static const char* HELL_VARIABLE_NAMES[] = {
	"reg_a", "reg_b", "reg_c", "reg_d",
	"reg_bp", "reg_sp", "src", "dst",
	"alu_src", "alu_dst", "tmp", "carry", "val_1222",
	"tmp2", "tmp3", "" // NULL instead of "" crashes 8cc
};
// --> labels: opr_reg_a, rot_reg_a, reg_a; opr_reg_b, ...; .....

static int num_hell_variable_return_labels[] = {
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0, 0,
	0, 0
};

static int num_flags = 0;
static int num_rotwidth_loop_calls = 0;
static int num_rw_var_calls = 0;


typedef struct {
	const char* function_name;
	const char* flag_name;
} HeLLFunction;


typedef enum {
	HELL_GENERATE_1222=0,
	HELL_INCREMENT=1,
	HELL_DECREMENT=2,
	HELL_ADD=3,
	HELL_SUB=4,
	HELL_OPR_MEMPTR=5,
	HELL_OPR_MEMORY=6,
	HELL_SET_MEMPTR=7,
	HELL_READ_MEMORY=8,
	HELL_WRITE_MEMORY=9,
	HELL_COMPUTE_MEMPTR=10,
	HELL_TEST_LT=11,
	HELL_TEST_GE=12,
	HELL_TEST_EQ=13,
	HELL_TEST_NEQ=14,
	HELL_TEST_GT=15,
	HELL_TEST_LE=16,
	HELL_MODULO=17
} HeLLFunctions;

static const HeLLFunction HELL_FUNCTIONS[] = {
	{"generate_1222", "GENERATE_1222_"},
	{"increment", "INCREMENT"},
	{"decrement", "DECREMENT"},
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
	{"modulo", "MODULO"}
};

static int num_calls[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};



// init
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
static void emit_sub_base(); // arithmetic: sub ALU_SRC from ALU_DST; ALU_SRC gets destroyed!; tmp_IS_C1-flag: overflow
static void emit_increment_base(); // arithmetic: increment ALU_DST by one; tmp_IS_C1-flag: overflow
static void emit_decrement_base(); // arithmetic: decrement ALU_DST by one; tmp_IS_C1-flag: overflow
static void emit_generate_val_1222_base(); // set VAL_1222 to 1t22.22
static void emit_rotwidth_loop_base();
static void emit_function_footer(HeLLFunctions hf); // generate function footer
static void finalize_hell();
static void init_state_hell(Data* data);


// use
static void emit_call(HeLLFunctions hf); // call some function
static void emit_read_0tvar(HellVariable var); // set VAL_1222 to 1t22..22, read a value starting with 0t from var
static void emit_read_1tvar(HellVariable var); // set VAL_1222 to 1t22..22, read a value starting with 1t from var
static void emit_clear_var(HellVariable var); // set var AND tmp to C1, prepare for writing...
static void emit_write_var(HellVariable var); // write to var; tmp and var must be set to C1 before
static void emit_opr_var(HellVariable var); // OPR var
static void emit_rot_var(HellVariable var); // ROT var
static void emit_test_var(HellVariable var); // MOVD var
static void emit_rotwidth_loop_begin(); // for (int i=0; i<rotwidth; i++) {
static void emit_rotwidth_loop_end();   // }


// helper
static void dec_ternary_string(char* str);




static void emit_call(HeLLFunctions hf) {
	num_calls[hf]++;
	emit_line("R_%s%u",HELL_FUNCTIONS[hf].flag_name,num_calls[hf]);
	emit_line("MOVD %s",HELL_FUNCTIONS[hf].function_name);
	dec_indent();
	emit_line("");
	emit_line("%s_ret%u:",HELL_FUNCTIONS[hf].function_name,num_calls[hf]);
	inc_indent();
}


static void emit_function_footer(HeLLFunctions hf) {
	for (int i=1; i<=num_calls[hf]; i++) {
		emit_line("%s%u %s_ret%u R_%s%u",HELL_FUNCTIONS[hf].flag_name,i,HELL_FUNCTIONS[hf].function_name,i,HELL_FUNCTIONS[hf].flag_name,i);
	}
	if (!num_calls[hf]) { emit_line("0"); /* fix LMFAO problem */ }
	emit_line("");
	emit_line(".CODE");
	for (int i=1; i<=num_calls[hf]; i++) {
		emit_line("%s%u:",HELL_FUNCTIONS[hf].flag_name,i);
		inc_indent();
		emit_line("Nop/MovD");
		emit_line("Jmp");
		dec_indent();
		emit_line("");
	}
}



// set VAL_1222 to 1t22..22, read from var
static void emit_read_0tvar(HellVariable var) {
	if (var == TMP || var == CARRY || var == VAL_1222) {
		error("oops");
	}
#if GENERATE_1222_ONLY_ONCE == 0
	emit_call(HELL_GENERATE_1222);
#endif
	num_rw_var_calls++;
	emit_line("do_read_again_%u:",num_rw_var_calls);
	emit_line("ROT C0 R_ROT");
	emit_opr_var(VAL_1222);
	emit_opr_var(var);
	emit_line("LOOP2 do_read_again_%u",num_rw_var_calls);
}

// set VAL_1222 to 1t22..22, read from var, but with preceeding 1t11..
static void emit_read_1tvar(HellVariable var) {
	if (var == TMP || var == CARRY || var == VAL_1222) {
		error("oops");
	}
#if GENERATE_1222_ONLY_ONCE == 0
	emit_call(HELL_GENERATE_1222);
#endif
	num_rw_var_calls++;
	emit_line("do_read_again_%u:",num_rw_var_calls);
	emit_line("ROT C1 R_ROT");
	emit_opr_var(VAL_1222);
	emit_opr_var(var);
	emit_line("LOOP2 do_read_again_%u",num_rw_var_calls);
}

// set var AND tmp to C1, prepare for writing...
static void emit_clear_var(HellVariable var) {
	if (var == TMP || var == VAL_1222) {
		error("oops");
	}
	emit_line("ROT C1 R_ROT");
	num_rw_var_calls++;
	emit_line("do_read_again_%u:",num_rw_var_calls);
	emit_opr_var(var);
	emit_line("LOOP2 do_read_again_%u",num_rw_var_calls);
	num_rw_var_calls++;
	emit_line("do_read_again_%u:",num_rw_var_calls);
	emit_opr_var(TMP);
	emit_line("LOOP2 do_read_again_%u",num_rw_var_calls);
}

// write to var; tmp and var must be set to C1 before
static void emit_write_var(HellVariable var) {
	if (var == TMP || var == CARRY || var == VAL_1222) {
		error("oops");
	}
	emit_opr_var(TMP);
	emit_opr_var(var);
}


static void emit_modulo_base() {
	emit_line(".DATA");
	emit_line("modulo:");
	emit_line("R_MOVD");

	// save modulus
	emit_clear_var(TMP3);
	emit_read_0tvar(ALU_SRC);
	emit_write_var(TMP3);

	emit_line("continue_modulo:");
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
	emit_line("R_%s_IS_C1", HELL_VARIABLE_NAMES[TMP]);
	emit_line("%s_IS_C1 continue_modulo", HELL_VARIABLE_NAMES[TMP]); // try one more

	// restore remainder

	emit_clear_var(ALU_DST);
	emit_read_0tvar(TMP2);
	emit_write_var(ALU_DST);

	emit_function_footer(HELL_MODULO);
}


static void emit_test_le_base() {
	emit_line(".DATA");
	emit_line("test_le:");
	emit_line("R_MOVD");
	
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
	emit_line(".DATA");
	emit_line("test_gt:");
	emit_line("R_MOVD");
	
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
	emit_line(".DATA");
	emit_line("test_neq:");
	emit_line("R_MOVD");

	emit_call(HELL_SUB);
	emit_clear_var(ALU_SRC);
	emit_line("ROT C1 R_ROT");
	emit_line("OPR 0t2 R_OPR");
	emit_line("OPR 1t0 R_OPR");
	emit_opr_var(ALU_SRC);

	emit_call(HELL_SUB);
	emit_clear_var(ALU_DST);
	emit_line("ROT C1 R_ROT");
	emit_line("%s_IS_C1 is_eq", HELL_VARIABLE_NAMES[TMP]);
	emit_line("OPR 0t2 R_OPR");
	emit_line("OPR 1t0 R_OPR");
	emit_line("is_eq:");
	emit_opr_var(ALU_DST);

	emit_function_footer(HELL_TEST_NEQ);
}


static void emit_test_eq_base() {
	emit_line(".DATA");
	emit_line("test_eq:");
	emit_line("R_MOVD");

	emit_call(HELL_SUB);
	emit_clear_var(ALU_SRC);
	emit_line("ROT C1 R_ROT");
	emit_line("OPR 0t2 R_OPR");
	emit_line("OPR 1t0 R_OPR");
	emit_opr_var(ALU_SRC);

	emit_call(HELL_SUB);
	emit_clear_var(ALU_DST);
	emit_line("ROT C1 R_ROT");
	emit_line("R_%s_IS_C1", HELL_VARIABLE_NAMES[TMP]);
	emit_line("%s_IS_C1 is_neq", HELL_VARIABLE_NAMES[TMP]);
	emit_line("OPR 0t2 R_OPR");
	emit_line("OPR 1t0 R_OPR");
	emit_line("is_neq:");
	emit_opr_var(ALU_DST);

	emit_function_footer(HELL_TEST_EQ);
}


static void emit_test_ge_base() {
	emit_line(".DATA");
	emit_line("test_ge:");
	emit_line("R_MOVD");

	emit_call(HELL_SUB);
	emit_clear_var(ALU_DST);
	emit_line("ROT C1 R_ROT");
	emit_line("%s_IS_C1 is_lt", HELL_VARIABLE_NAMES[TMP]);
	emit_line("OPR 0t2 R_OPR");
	emit_line("OPR 1t0 R_OPR");
	emit_line("is_lt:");
	emit_opr_var(ALU_DST);

	emit_function_footer(HELL_TEST_GE);
}


static void emit_test_lt_base() {
	emit_line(".DATA");
	emit_line("test_lt:");
	emit_line("R_MOVD");

	emit_call(HELL_SUB);
	emit_clear_var(ALU_DST);
	emit_line("ROT C1 R_ROT");
	emit_line("R_%s_IS_C1", HELL_VARIABLE_NAMES[TMP]);
	emit_line("%s_IS_C1 is_ge", HELL_VARIABLE_NAMES[TMP]);
	emit_line("OPR 0t2 R_OPR");
	emit_line("OPR 1t0 R_OPR");
	emit_line("is_ge:");
	emit_opr_var(ALU_DST);

	emit_function_footer(HELL_TEST_LT);
}




static void emit_memory_access_base() {
	emit_line(".DATA");
	emit_line("");
	emit_line("opr_memory:");
	emit_line("U_MOVDOPRMOVD memptr");
	emit_line("opr_memptr:");
	emit_line("OPR");
	emit_line("memptr:");
	emit_line("MEMORY_0 - 2");
	emit_line("R_OPR");
	emit_line("R_MOVD");
	emit_function_footer(HELL_OPR_MEMPTR);

	emit_line(".DATA");
	emit_line("");
	emit_line("restore_opr_memory:");
	emit_line("R_MOVD");
	emit_line("restore_opr_memory_no_r_moved:");
	emit_line("PARTIAL_MOVDOPRMOVD ?-");
	emit_line("R_MOVDOPRMOVD ?- ?- ?- ?-");
	emit_line("LOOP4 half_of_restore_opr_memory_done");
	emit_line("MOVD restore_opr_memory");
	emit_line("");
	emit_line("half_of_restore_opr_memory_done:");
	emit_line("LOOP2_2 restore_opr_memory_done");
	emit_line("PARTIAL_MOVDOPRMOVD");
	emit_line("restore_opr_memory_no_r_moved");
	emit_line("");
	emit_line("restore_opr_memory_done:");
	emit_function_footer(HELL_OPR_MEMORY);

	emit_line(".CODE");
	emit_line("LOOP4:");
	emit_line("Nop/Nop/Nop/MovD");
	emit_line("Jmp");
	emit_line("");
	emit_line("LOOP2_2:");
	emit_line("Nop/MovD");
	emit_line("Jmp");
	emit_line("");
	emit_line("MOVDOPRMOVD:");
	emit_line("MovD/Nop/Nop/Nop/Nop/Nop/Nop/Nop/Nop");
	emit_line("RNop");
	emit_line("RNop");
	emit_line("Opr/Nop/Nop/Nop/Nop/Rot/Nop/Nop/Nop");
	emit_line("PARTIAL_MOVDOPRMOVD:");
	emit_line("MovD/Nop/Nop/Nop/Nop/Nop/Nop/Nop/Nop");
	emit_line("Jmp");
	emit_line("");
}


static void emit_compute_memptr_base() {
	; // ALU_DST := (MEMORY_0 - 2) - 2* ALU_SRC
	emit_line(".DATA");
	emit_line("compute_memptr:");
	emit_line("R_MOVD");
	// set ALU_DST to MEMORY_0-2
#if GENERATE_1222_ONLY_ONCE == 0
	emit_call(HELL_GENERATE_1222);
#endif
	emit_clear_var(ALU_DST);
	num_rw_var_calls++;
	emit_line("do_read_again_%u:",num_rw_var_calls);
	emit_line("ROT C1 R_ROT");
	emit_opr_var(VAL_1222);
	emit_line("OPR MEMORY_0-2 R_OPR");
	emit_line("LOOP2 do_read_again_%u",num_rw_var_calls);
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
	emit_line(".DATA");
	emit_line("write_memory:");
	emit_line("R_MOVD");
	emit_line("ROT C1 R_ROT");
	emit_call(HELL_OPR_MEMORY);
	emit_call(HELL_OPR_MEMORY);
	emit_opr_var(TMP);
	emit_opr_var(TMP);
	emit_read_0tvar(ALU_DST);
	emit_opr_var(TMP);
	emit_call(HELL_OPR_MEMORY);
	emit_function_footer(HELL_WRITE_MEMORY);
}


static void emit_read_memory_base() {
	emit_line(".DATA");
	emit_line("read_memory:");
	emit_line("R_MOVD");
#if GENERATE_1222_ONLY_ONCE == 0
	emit_call(HELL_GENERATE_1222);
#endif
	emit_line("ROT C1 R_ROT");
	emit_opr_var(ALU_DST);
	emit_opr_var(ALU_DST);
	emit_opr_var(TMP);
	emit_opr_var(TMP);

	num_rw_var_calls++;
	emit_line("do_read_again_%u:",num_rw_var_calls);
	emit_line("ROT C0 R_ROT");
	emit_opr_var(VAL_1222);
	emit_call(HELL_OPR_MEMORY);
	emit_line("LOOP2 do_read_again_%u",num_rw_var_calls);

	emit_opr_var(TMP);
	emit_opr_var(ALU_DST);
	emit_function_footer(HELL_READ_MEMORY);
}


static void emit_set_memptr_base() {
	emit_line(".DATA");
	emit_line("set_memptr:");
	emit_line("R_MOVD");
	emit_line("ROT C1 R_ROT");
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
// if tmp_IS_C1-flag is set:
//  overflow occured. -> calling method should handle this
//  possible overflow handling: undo increment, increase rot-width, try increment again

	emit_line(".DATA");
	emit_line("add:");
	inc_indent();
	emit_line("%s_IS_C1 add",HELL_VARIABLE_NAMES[TMP]); // set CARRY flag
	emit_line("R_%s_IS_C1",HELL_VARIABLE_NAMES[TMP]); // clear CARRY flag
	emit_line("R_MOVD");
#if GENERATE_1222_ONLY_ONCE == 0
	emit_call(HELL_GENERATE_1222);
#endif
  emit_rotwidth_loop_begin();
// LOOP: //
	// if CARRY
	emit_line("R_%s_IS_C1",HELL_VARIABLE_NAMES[TMP]);
	emit_line("%s_IS_C1 no_increment_during_add",HELL_VARIABLE_NAMES[TMP]);
	//  unset CARRY and increment ALU_DST
	emit_line("R_%s_IS_C1",HELL_VARIABLE_NAMES[TMP]);
	emit_line("force_increment_during_add:");
	emit_clear_var(CARRY);
	emit_opr_var(TMP); // set tmp to C0
	emit_opr_var(VAL_1222); // load 1t22..22
	emit_opr_var(CARRY); // set carry to 0t22..22
	emit_line("ROT C1 R_ROT");
	emit_line("OPR 0t2 R_OPR");
	emit_opr_var(CARRY); // set carry to 0t22..21
	emit_line("ROT C1 R_ROT");
	emit_opr_var(VAL_1222); // load 0t22..22
	num_rw_var_calls++;
	emit_line("do_read_again_%u:",num_rw_var_calls);
	emit_opr_var(ALU_DST); // opr dest
	emit_opr_var(TMP); // opr tmp
	emit_opr_var(CARRY); // opr carry
	emit_line("LOOP2 do_read_again_%u",num_rw_var_calls);
	num_rw_var_calls++;
	emit_line("do_read_again_%u:",num_rw_var_calls);
	emit_line("ROT C1 R_ROT");
	emit_line("OPR 0t2 R_OPR"); // load 0t2
	emit_opr_var(TMP); // opr tmp
	emit_line("LOOP2 do_read_again_%u",num_rw_var_calls);
	emit_line("%s_IS_C1 keep_carry_during_add",HELL_VARIABLE_NAMES[TMP]); // keep increment carry-flag
	emit_test_var(TMP); // MOVD tmp
	emit_line("R_%s_IS_C1",HELL_VARIABLE_NAMES[TMP]);
	emit_line("keep_carry_during_add:");
	emit_line("R_%s_IS_C1",HELL_VARIABLE_NAMES[TMP]);
	emit_line("no_increment_during_add:");

	// decrement ALU_SRC
	emit_clear_var(CARRY);
	emit_opr_var(TMP); // set tmp to C0
	emit_opr_var(VAL_1222); // load 1t22..22
	emit_opr_var(CARRY); // set carry to 0t22..22
	emit_line("ROT C1 R_ROT");
	emit_line("OPR 0t2 R_OPR");
	emit_opr_var(CARRY); // set carry to 1t22..21
	emit_line("ROT C1 R_ROT");
	emit_opr_var(VAL_1222); // load 0t22..22
	emit_opr_var(ALU_SRC); // OPR src
	emit_line("ROT C1 R_ROT");
	emit_opr_var(VAL_1222); // load 0t22..22
	emit_opr_var(ALU_SRC); //   OPR src
	emit_opr_var(TMP); //   OPR tmp
	emit_opr_var(CARRY); //   OPR carry
	emit_opr_var(ALU_SRC); //   OPR src
	emit_line("ROT C1 R_ROT");
	emit_opr_var(VAL_1222); // load 0t22..22
	emit_opr_var(ALU_SRC); //   OPR src
	emit_line("ROT C1 R_ROT");
	emit_opr_var(VAL_1222); // load 0t22..22
	emit_opr_var(CARRY); //   OPR carry
	emit_line("ROT C1 R_ROT");
	emit_line("OPR 0t2 R_OPR"); // load 0t2
	emit_opr_var(CARRY); //   OPR carry
	emit_test_var(CARRY); // MOVD carry
	// IF NO BORROW OCURRED: increment ALU_DST
	emit_line("%s_IS_C1 force_increment_during_add",HELL_VARIABLE_NAMES[CARRY]); //  GOTO force_increment if CARRY flag is set (=NO BORROW)

	emit_rot_var(ALU_DST); // rot dest
	emit_rot_var(ALU_SRC); // rot src
	emit_rotwidth_loop_end();
	// emit_line("ROT C0 R_ROT");
	// emit_opr_var(VAL_1222); // restore VAL_1222 to 1t22..22 (instead of 0t22..22)
	dec_indent();
	emit_function_footer(HELL_ADD);
}



static void emit_sub_base() {
// if tmp_IS_C1-flag is set:
//  overflow occured. -> calling method should handle this
//  possible overflow handling: undo increment, increase rot-width, try increment again

	emit_line(".DATA");
	emit_line("sub:");
	inc_indent();
	emit_line("%s_IS_C1 sub",HELL_VARIABLE_NAMES[TMP]); // set CARRY flag
	emit_line("R_%s_IS_C1",HELL_VARIABLE_NAMES[TMP]); // clear CARRY flag
	emit_line("R_MOVD");
#if GENERATE_1222_ONLY_ONCE == 0
	emit_call(HELL_GENERATE_1222);
#endif
  emit_rotwidth_loop_begin();
// LOOP: //
	// if CARRY
	emit_line("R_%s_IS_C1",HELL_VARIABLE_NAMES[TMP]);
	emit_line("%s_IS_C1 no_decrement_during_sub",HELL_VARIABLE_NAMES[TMP]);
	//  unset CARRY and increment ALU_DST
	emit_line("R_%s_IS_C1",HELL_VARIABLE_NAMES[TMP]);
	emit_line("force_decrement_during_sub:");

	// decrement ALU_DST
	emit_clear_var(CARRY);
	emit_opr_var(TMP); // set tmp to C0
	emit_opr_var(VAL_1222); // load 1t22..22
	emit_opr_var(CARRY); // set carry to 0t22..22
	emit_line("ROT C1 R_ROT");
	emit_line("OPR 0t2 R_OPR");
	emit_opr_var(CARRY); // set carry to 1t22..21
	emit_line("ROT C1 R_ROT");
	emit_opr_var(VAL_1222); // load 0t22..22
	emit_opr_var(ALU_DST); // OPR src
	emit_line("ROT C1 R_ROT");
	emit_opr_var(VAL_1222); // load 0t22..22
	emit_opr_var(ALU_DST); //   OPR src
	emit_opr_var(TMP); //   OPR tmp
	emit_opr_var(CARRY); //   OPR carry
	emit_opr_var(ALU_DST); //   OPR src
	emit_line("ROT C1 R_ROT");
	emit_opr_var(VAL_1222); // load 0t22..22
	emit_opr_var(ALU_DST); //   OPR src
	emit_line("ROT C1 R_ROT");
	emit_opr_var(VAL_1222); // load 0t22..22
	emit_opr_var(CARRY); //   OPR carry
	emit_line("ROT C1 R_ROT");
	emit_line("OPR 0t2 R_OPR"); // load 0t2
	emit_opr_var(CARRY); //   OPR carry
	emit_test_var(CARRY); // MOVD carry

	emit_line("%s_IS_C1 keep_carry_during_sub",HELL_VARIABLE_NAMES[TMP]); // keep increment carry-flag
	emit_test_var(CARRY); // MOVD tmp
	emit_line("%s_IS_C1 keep_carry_during_sub",HELL_VARIABLE_NAMES[CARRY]); // if CARRY_IS_C1 is set, TMP_IS_C1 should remain disabled
	emit_line("R_%s_IS_C1",HELL_VARIABLE_NAMES[TMP]);
	emit_line("keep_carry_during_sub:");
	emit_line("R_%s_IS_C1",HELL_VARIABLE_NAMES[TMP]);
	emit_line("no_decrement_during_sub:");

	// decrement ALU_SRC
	emit_clear_var(CARRY);
	emit_opr_var(TMP); // set tmp to C0
	emit_opr_var(VAL_1222); // load 1t22..22
	emit_opr_var(CARRY); // set carry to 0t22..22
	emit_line("ROT C1 R_ROT");
	emit_line("OPR 0t2 R_OPR");
	emit_opr_var(CARRY); // set carry to 1t22..21
	emit_line("ROT C1 R_ROT");
	emit_opr_var(VAL_1222); // load 0t22..22
	emit_opr_var(ALU_SRC); // OPR src
	emit_line("ROT C1 R_ROT");
	emit_opr_var(VAL_1222); // load 0t22..22
	emit_opr_var(ALU_SRC); //   OPR src
	emit_opr_var(TMP); //   OPR tmp
	emit_opr_var(CARRY); //   OPR carry
	emit_opr_var(ALU_SRC); //   OPR src
	emit_line("ROT C1 R_ROT");
	emit_opr_var(VAL_1222); // load 0t22..22
	emit_opr_var(ALU_SRC); //   OPR src
	emit_line("ROT C1 R_ROT");
	emit_opr_var(VAL_1222); // load 0t22..22
	emit_opr_var(CARRY); //   OPR carry
	emit_line("ROT C1 R_ROT");
	emit_line("OPR 0t2 R_OPR"); // load 0t2
	emit_opr_var(CARRY); //   OPR carry
	emit_test_var(CARRY); // MOVD carry
	// IF NO BORROW OCURRED: increment ALU_DST
	emit_line("%s_IS_C1 force_decrement_during_sub",HELL_VARIABLE_NAMES[CARRY]); //  GOTO force_increment if CARRY flag is set (=NO BORROW)

	emit_rot_var(ALU_DST); // rot dest
	emit_rot_var(ALU_SRC); // rot src
	emit_rotwidth_loop_end();
	// emit_line("ROT C0 R_ROT");
	// emit_opr_var(VAL_1222); // restore VAL_1222 to 1t22..22 (instead of 0t22..22)
	dec_indent();
	emit_function_footer(HELL_SUB);
}



static void emit_increment_base() {
// if tmp_IS_C1-flag is set:
//  overflow occured. -> calling method should handle this
//  possible overflow handling: undo increment, increase rot-width, try increment again

	emit_line(".DATA");
	emit_line("increment:");
	inc_indent();
	emit_line("%s_IS_C1 increment",HELL_VARIABLE_NAMES[TMP]); // set CARRY flag
	emit_line("R_MOVD");
#if GENERATE_1222_ONLY_ONCE == 0
	emit_call(HELL_GENERATE_1222);
#endif
  emit_rotwidth_loop_begin();
// LOOP: //
	// if CARRY
	emit_line("R_%s_IS_C1",HELL_VARIABLE_NAMES[TMP]);
	emit_line("%s_IS_C1 no_increment",HELL_VARIABLE_NAMES[TMP]);
	emit_clear_var(CARRY);
	emit_opr_var(TMP); // set tmp to C0
	emit_opr_var(VAL_1222); // load 1t22..22
	emit_opr_var(CARRY); // set carry to 0t22..22
	emit_line("ROT C1 R_ROT");
	emit_line("OPR 0t2 R_OPR");
	emit_opr_var(CARRY); // set carry to 0t22..21
	emit_line("ROT C1 R_ROT");
	emit_opr_var(VAL_1222); // load 0t22..22
	num_rw_var_calls++;
	emit_line("do_read_again_%u:",num_rw_var_calls);
	emit_opr_var(ALU_DST); // opr dest
	emit_opr_var(TMP); // opr tmp
	emit_opr_var(CARRY); // opr carry
	emit_line("LOOP2 do_read_again_%u",num_rw_var_calls);
	num_rw_var_calls++;
	emit_line("do_read_again_%u:",num_rw_var_calls);
	emit_line("ROT C1 R_ROT");
	emit_line("OPR 0t2 R_OPR"); // load 0t2
	emit_opr_var(TMP); // opr tmp
	emit_line("LOOP2 do_read_again_%u",num_rw_var_calls);
	emit_test_var(TMP); // MOVD tmp
	emit_line("no_increment:");
	emit_rot_var(ALU_DST); // rot dest
	emit_rotwidth_loop_end();
	// emit_line("ROT C0 R_ROT");
	// emit_opr_var(VAL_1222); // restore VAL_1222 to 1t22..22 (instead of 0t22..22)
	dec_indent();
	emit_function_footer(HELL_INCREMENT);
}


static void emit_decrement_base() {
// if tmp_IS_C1-flag is set:
//  overflow occured. -> calling method should handle this
//  possible overflow handling: undo decrement, increase rot-width, try decrement again

	emit_line(".DATA");
	emit_line("decrement:");
	inc_indent();
	emit_line("%s_IS_C1 decrement",HELL_VARIABLE_NAMES[TMP]); // set CARRY flag
	emit_line("R_MOVD");
#if GENERATE_1222_ONLY_ONCE == 0
	emit_call(HELL_GENERATE_1222);
#endif
  emit_rotwidth_loop_begin();
// LOOP: //
	// if CARRY
	emit_line("R_%s_IS_C1",HELL_VARIABLE_NAMES[TMP]);
	emit_line("%s_IS_C1 no_decrement",HELL_VARIABLE_NAMES[TMP]);
	emit_clear_var(CARRY);
	emit_opr_var(TMP); // set tmp to C0
	emit_opr_var(VAL_1222); // load 1t22..22
	emit_opr_var(CARRY); // set carry to 0t22..22
	emit_line("ROT C1 R_ROT");
	emit_line("OPR 0t2 R_OPR");
	emit_opr_var(CARRY); // set carry to 1t22..21
	emit_line("ROT C1 R_ROT");
	emit_opr_var(VAL_1222); // load 0t22..22
	emit_opr_var(ALU_DST); // OPR src
	emit_line("ROT C1 R_ROT");
	emit_opr_var(VAL_1222); // load 0t22..22
	emit_opr_var(ALU_DST); //   OPR src
	emit_opr_var(TMP); //   OPR tmp
	emit_opr_var(CARRY); //   OPR carry
	emit_opr_var(ALU_DST); //   OPR src
	emit_line("ROT C1 R_ROT");
	emit_opr_var(VAL_1222); // load 0t22..22
	emit_opr_var(ALU_DST); //   OPR src
	emit_line("ROT C1 R_ROT");
	emit_opr_var(VAL_1222); // load 0t22..22
	emit_opr_var(CARRY); //   OPR carry
	emit_line("ROT C1 R_ROT");
	emit_line("OPR 0t2 R_OPR"); // load 0t2
	emit_opr_var(CARRY); //   OPR carry
	emit_test_var(CARRY); // MOVD carry

	emit_test_var(CARRY); // MOVD tmp
	emit_line("%s_IS_C1 destroy_carry_during_decrement",HELL_VARIABLE_NAMES[CARRY]); // if CARRY_IS_C1 is set, TMP_IS_C1 should be disabled
	emit_line("R_%s_IS_C1",HELL_VARIABLE_NAMES[TMP]);
	emit_line("destroy_carry_during_decrement:");
	emit_line("R_%s_IS_C1",HELL_VARIABLE_NAMES[TMP]);

	emit_line("no_decrement:");
	emit_rot_var(ALU_DST); // rot dest
	emit_rotwidth_loop_end();
	// emit_line("ROT C0 R_ROT");
	// emit_opr_var(VAL_1222); // restore VAL_1222 to 1t22..22 (instead of 0t22..22)
	dec_indent();
	emit_function_footer(HELL_DECREMENT);
}



static void emit_generate_val_1222_base() {
	emit_line(".DATA");
	emit_line("generate_1222:");
	inc_indent();
	emit_line("R_MOVD");
	emit_line("ROT C1 R_ROT");
	emit_line("generate_1222_2loop:");
	emit_opr_var(VAL_1222);
	emit_line("LOOP2 generate_1222_2loop");
	emit_rotwidth_loop_begin();
	emit_line("ROT C1 R_ROT");
	emit_line("OPR 0t2 R_OPR");
	emit_opr_var(VAL_1222);
	emit_rot_var(VAL_1222); ////
	emit_rotwidth_loop_end();
	dec_indent();
	emit_function_footer(HELL_GENERATE_1222);
}



static void emit_opr_var(HellVariable var) {
	num_hell_variable_return_labels[var]++;
	emit_line("R_%s_RETURN%u",HELL_VARIABLE_NAMES[var],num_hell_variable_return_labels[var]);
	emit_line("MOVD opr_%s",HELL_VARIABLE_NAMES[var]);
	emit_line("");
	emit_line("%s_ret%u:",HELL_VARIABLE_NAMES[var],num_hell_variable_return_labels[var]);
}

static void emit_rot_var(HellVariable var) {
	num_hell_variable_return_labels[var]++;
	emit_line("R_%s_RETURN%u",HELL_VARIABLE_NAMES[var],num_hell_variable_return_labels[var]);
	emit_line("MOVD rot_%s",HELL_VARIABLE_NAMES[var]);
	emit_line("");
	emit_line("%s_ret%u:",HELL_VARIABLE_NAMES[var],num_hell_variable_return_labels[var]);
}

static void emit_test_var(HellVariable var) {
	num_hell_variable_return_labels[var]++;
	emit_line("R_%s_RETURN%u",HELL_VARIABLE_NAMES[var],num_hell_variable_return_labels[var]);
	emit_line("MOVD %s",HELL_VARIABLE_NAMES[var]);
	emit_line("");
	emit_line("%s_ret%u:",HELL_VARIABLE_NAMES[var],num_hell_variable_return_labels[var]);
}



static void emit_rotwidth_loop_begin() {
	num_rotwidth_loop_calls++;
	emit_line("R_ROTWIDTH_LOOP%u",num_rotwidth_loop_calls);
	emit_line("MOVD loop");
	dec_indent();
	emit_line("");
	emit_line("rotwidth_loop_inner%u:",num_rotwidth_loop_calls);
	inc_indent();
	emit_line("R_ROTWIDTH_LOOP%u",num_rotwidth_loop_calls);
}
static void emit_rotwidth_loop_end() {
	emit_line("MOVD end_of_loop_body");
	dec_indent();
	emit_line("");
	emit_line("rotwidth_loop_ret%u:",num_rotwidth_loop_calls);
	inc_indent();
}

static void emit_rotwidth_loop_base() {
  /** loop over rotwidth */
  int flag_base = num_flags;
  num_flags += 4;
  
  emit_line(".DATA");
  emit_line("opr_loop_tmp:");
  inc_indent();
  emit_line("R_MOVD");
  emit_line("OPR");
  dec_indent();
  emit_line("loop_tmp:");
  inc_indent();
  emit_line("?");
  emit_line("U_NOP no_decision");
  emit_line("U_NOP was_C1");
  emit_line("U_NOP was_C10");
  dec_indent();
  emit_line("was_C1:");
  inc_indent();
  emit_line("R_LOOP2");
  emit_line("MOVD leave_loop");
  dec_indent();
  emit_line("was_C10:");
  inc_indent();
  emit_line("R_LOOP2");
  emit_line("MOVD loop");
  dec_indent();
  emit_line("no_decision:");
  inc_indent();
  emit_line("R_OPR");
  for (int i=1;i<=4;i++) {
    emit_line("FLAG%u loop_tmp_ret%u R_FLAG%u",flag_base+i,i,flag_base+i);
  }
  dec_indent();
  emit_line("");
  emit_line("loop:");
  inc_indent();
  emit_line("R_MOVD");
  for (int i=1;i<=num_rotwidth_loop_calls;i++){
    emit_line("ROTWIDTH_LOOP%u rotwidth_loop_inner%u R_ROTWIDTH_LOOP%u",i,i,i);
  }
  // emit_line("MOVD end_of_loop_body");
  emit_line("");
  dec_indent();
  emit_line("end_of_loop_body:");
  inc_indent();
  emit_line("R_MOVD");
  emit_line("ROT C1 R_ROT");
  dec_indent();
  emit_line("reset_loop_tmp_loop:");
  inc_indent();
  emit_line("R_FLAG%u",flag_base+1);
  emit_line("MOVD opr_loop_tmp");
  emit_line("");
  dec_indent();
  emit_line("loop_tmp_ret1:");
  inc_indent();
  emit_line("LOOP2 reset_loop_tmp_loop");
  dec_indent();
  emit_line("do_twice:");
  inc_indent();
  emit_line("ROT C1 R_ROT");
  emit_line("OPR C02 R_OPR");
  emit_line("R_FLAG%u",flag_base+2);
  emit_line("MOVD opr_loop_tmp");
  emit_line("");
  dec_indent();
  emit_line("loop_tmp_ret2:");
  inc_indent();
  emit_line("R_LOOP2");
  emit_line("LOOP2 loop_tmp");
  emit_line("R_FLAG%u",flag_base+3);
  emit_line("MOVD opr_loop_tmp");
  emit_line("");
  dec_indent();
  emit_line("loop_tmp_ret3:");
  inc_indent();
  emit_line("ROT C12 R_ROT");
  emit_line("R_FLAG%u",flag_base+4);
  emit_line("MOVD opr_loop_tmp");
  emit_line("");
  dec_indent();
  emit_line("loop_tmp_ret4:");
  inc_indent();
  emit_line("LOOP2 do_twice");
  emit_line("");
  dec_indent();
  emit_line("leave_loop:");
  inc_indent();
  emit_line("R_MOVD");
  for (int i=1;i<=num_rotwidth_loop_calls;i++){
    emit_line("ROTWIDTH_LOOP%u rotwidth_loop_ret%u R_ROTWIDTH_LOOP%u",i,i,i);
  }
  dec_indent();
  emit_line("");

  emit_line(".CODE");
  for (int i=1;i<=num_rotwidth_loop_calls;i++){
    emit_line("ROTWIDTH_LOOP%u:",i);
    inc_indent();
    emit_line("Nop/MovD");
    emit_line("Jmp");
    dec_indent();
    emit_line("");
  }
  emit_line("");

/** END: LOOP over rotwidth */
}


static void finalize_hell() {

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
  emit_increment_base();
  emit_decrement_base();
  emit_generate_val_1222_base();
  emit_rotwidth_loop_base();

  emit_line(".DATA");
  emit_line("");
	for (int i=0; HELL_VARIABLE_NAMES[i][0] != 0; i++) {
		if (!num_hell_variable_return_labels[i]) {
			// variable is not USED
			continue;
		}
		emit_line("opr_%s:",HELL_VARIABLE_NAMES[i]);
		inc_indent();
		emit_line("R_ROT");
		emit_line("U_OPR %s",HELL_VARIABLE_NAMES[i]);
		dec_indent();
		emit_line("rot_%s:",HELL_VARIABLE_NAMES[i]);
		inc_indent();
		emit_line("R_OPR");
		emit_line("U_ROT %s",HELL_VARIABLE_NAMES[i]);
		dec_indent();
		emit_line("%s:",HELL_VARIABLE_NAMES[i]);
		inc_indent();
		emit_line("?");
		emit_line("U_NOP continue_%s",HELL_VARIABLE_NAMES[i]);
		emit_line("U_NOP %s_was_c1",HELL_VARIABLE_NAMES[i]);
		emit_line("U_NOP %s_was_c0",HELL_VARIABLE_NAMES[i]);
		dec_indent();
		emit_line("%s_was_c1:",HELL_VARIABLE_NAMES[i]);
		inc_indent();
		emit_line("%s_IS_C1 %s_was_c1",HELL_VARIABLE_NAMES[i],HELL_VARIABLE_NAMES[i]);
		emit_line("U_NOP return_from_%s",HELL_VARIABLE_NAMES[i]);
		dec_indent();
		emit_line("%s_was_c0:",HELL_VARIABLE_NAMES[i]);
		inc_indent();
		emit_line("%s_IS_C1 %s_was_c0",HELL_VARIABLE_NAMES[i],HELL_VARIABLE_NAMES[i]);
		emit_line("R_%s_IS_C1",HELL_VARIABLE_NAMES[i]);
		emit_line("U_NOP return_from_%s",HELL_VARIABLE_NAMES[i]);
		dec_indent();
		emit_line("continue_%s:",HELL_VARIABLE_NAMES[i]);
		inc_indent();
		emit_line("R_ROT R_OPR");
		dec_indent();
		emit_line("return_from_%s:",HELL_VARIABLE_NAMES[i]);
		inc_indent();
		emit_line("R_MOVD");
		for (int j=1;j<=num_hell_variable_return_labels[i];j++) {
			emit_line("%s_RETURN%u %s_ret%u R_%s_RETURN%u",HELL_VARIABLE_NAMES[i],j,HELL_VARIABLE_NAMES[i],j,HELL_VARIABLE_NAMES[i],j);
		}
		dec_indent();
		emit_line("");
	}

  emit_line(".CODE");
  emit_line("");

	for (int i=0; HELL_VARIABLE_NAMES[i][0] != 0; i++) {
		if (!num_hell_variable_return_labels[i]) {
			// variable is not USED
			continue;
		}
		emit_line("%s_IS_C1:",HELL_VARIABLE_NAMES[i]);
		inc_indent();
		emit_line("Nop/MovD");
		emit_line("Jmp");
		dec_indent();
		emit_line("");
		for (int j=1;j<=num_hell_variable_return_labels[i];j++) {
			emit_line("%s_RETURN%u:",HELL_VARIABLE_NAMES[i],j);
			inc_indent();
			emit_line("Nop/MovD");
			emit_line("Jmp");
			dec_indent();
			emit_line("");
		}
	}

  emit_line("MOVD:");
  inc_indent();
  emit_line("MovD/Nop");
  emit_line("Jmp");
  dec_indent();
  emit_line("");

  emit_line("ROT:");
  inc_indent();
  emit_line("Rot/Nop");
  emit_line("Jmp");
  dec_indent();
  emit_line("");

  emit_line("OPR:");
  inc_indent();
  emit_line("Opr/Nop");
  emit_line("Jmp");
  dec_indent();
  emit_line("");

  emit_line("IN:");
  inc_indent();
  emit_line("In/Nop");
  emit_line("Jmp");
  dec_indent();
  emit_line("");

  emit_line("OUT:");
  inc_indent();
  emit_line("Out/Nop");
  emit_line("Jmp");
  dec_indent();
  emit_line("");

  emit_line("NOP:");
  inc_indent();
  emit_line("Jmp");
  dec_indent();
  emit_line("");

  emit_line("HALT:");
  inc_indent();
  emit_line("Hlt");
  dec_indent();
  emit_line("");

  emit_line("LOOP2:");
  inc_indent();
  emit_line("MovD/Nop");
  emit_line("Jmp");
  dec_indent();
  emit_line("");

  for (int i=1; i<=num_flags; i++) {  
    emit_line("FLAG%u:",i);
    inc_indent();
    emit_line("Nop/MovD");
    emit_line("Jmp");
    dec_indent();
    emit_line("");
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
	/* THS MUST BE WRITTEN AT THE VERY START OF THE PROGRAM ---- TODO: maybe not??*/
	emit_line(".DATA");
	emit_line("");
	emit_line("// backjump");
	emit_line("@1t01112 return_from_uninitialized_cell:");
	emit_line("R_MOVD");
	emit_line("MOVD restore_opr_memory");
	emit_line("");
	emit_line("// memory array");
	
	char address[20];
	for (int i=0; i<13; i++)
		address[i] = '1';
	address[13] = '0';
	address[14] = '2';
	address[15] = '2';
	address[16] = '2';
	address[17] = '2';
	address[18] = '2';
	address[19] = 0;
	
	
	int mp = 0;
	for (; data; data = data->next, mp++) {
		emit_line("@1t%s",address);
		emit_line("MEMORY_%u:", mp);
		if (data->v) {
			emit_line("%u return_from_uninitialized_cell", data->v);
		}else{
			emit_line("? return_from_uninitialized_cell");
		}
		emit_line("");
		dec_ternary_string(address); dec_ternary_string(address);
	}
	if (!mp) {
		emit_line("@1t022222");
		emit_line("MEMORY_0:");
		emit_line("? return_from_uninitialized_cell");
		emit_line("");
	}
	
  // TODO: necessary?? sufficient large value?
  emit_line("unused: %u*9 // force rot_width to be large enough", UINT_MAX); // multiplication with 3 may not be necessary
  emit_line("");


  if (data) {
  }
}


void target_hell(Module* module) {
  init_state_hell(module->data);

  if (module->text) { }

  emit_line(".DATA");
  emit_line("ENTRY:");
#if GENERATE_1222_ONLY_ONCE != 0
	emit_call(HELL_GENERATE_1222);
#endif


	inc_indent();


	emit_clear_var(ALU_SRC);
	emit_line("ROT 1t222221 R_ROT OPR 1t22222!'~' R_OPR");
	emit_write_var(ALU_SRC);


	emit_clear_var(ALU_DST);
	emit_line("ROT 1t2222222222222221 R_ROT OPR 1t222222222222222!('h'+27*'~') R_OPR");
	emit_write_var(ALU_DST);
	
	emit_call(HELL_MODULO);

	emit_clear_var(ALU_SRC);
	emit_line("ROT 1t222221 R_ROT OPR 1t22222!'i' R_OPR");
	emit_write_var(ALU_SRC);

	emit_read_0tvar(ALU_DST);
	emit_line("OUT ?- R_OUT");
	emit_read_0tvar(ALU_SRC);
	emit_line("OUT ?- R_OUT");
	
	emit_call(HELL_TEST_NEQ);

	emit_clear_var(ALU_SRC);
	emit_line("ROT 1t222221 R_ROT OPR 1t22222!('!'-1) R_OPR");
	emit_write_var(ALU_SRC);
	
	emit_call(HELL_ADD);

	emit_read_0tvar(ALU_DST);
	emit_line("OUT ?- R_OUT");


  emit_line("ROT 1t222221 R_ROT OPR 1t22222!'\\n' R_OPR");
  emit_line("OUT ?- R_OUT");
  emit_line("HALT");
  dec_indent();
  emit_line("");

  finalize_hell();
}
