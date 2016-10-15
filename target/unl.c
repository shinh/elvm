#include <stdio.h>
#include <ir/ir.h>
#include <target/util.h>
#include <target/unlcore.h>

static const char S2[] = "``s";
static const char K1[] = "`k";
static const char CONS[] = "``s``s`ks``s`kk``s`ks``s`k`sik`kk";
static const char CAR[] = "``si`kk";
static const char CDR[] = "``si`k`ki";
static const char DOTCAR[] = "k";   // (x DOTCAR) = (CAR x)

// (CONS_KI x) = (cons (K I) x)
static const char CONS_KI[] = "``s`k`s``si`k`kik";

// (CONS_K x) = (cons K x)
static const char CONS_K[] = "``s`k`s``si`kkk";

// (APPLY_CDR f lst) = (cons (car lst) (f (cdr lst)))
static const char APPLY_CDR[] = "``s`k`si``s`kk``s`k`s``s`ks``s`kk``s`ks``s`k`sik``s`kk`s`kk";

// (REPLACE_CAR x lst) = (cons x (cdr lst))
static const char REPLACE_CAR[] = "``s`k`si``s`kk``s`kk``s`k`s``s`k`s``s`ks``s`k`sik``s`kkkk";

// ([REPLACE_CAR1 + <expr>] lst) = (cons <expr> (cdr lst))
static const char REPLACE_CAR1[] = "``si`k`k``s``s`k`s``s`ks``s`k`sik``s`kkk`k";

// (COMPOSE f g x) = (f (g x))
static const char COMPOSE[] = "``s`ksk";

// (n POST_INC) = n+1
static const char POST_INC[] = "```sii``s``s`ks``s`k`si``s`kk``s`k`s`k``s`k`s``si`k`kik``s`k`si``s``s`kskk`k`k``s`k`s``si`kkk";

// (n POST_DEC) = n-1
static const char POST_DEC[] = "```sii``s`k`s``si`k``s`k`s``si`k`kik``s`kk``s`k`s`k``s`k`s``si`kkk``s`k`si``s``s`kskk";

// Church numers up to 24
static const char* CHURCHNUM[] = {
  "`ki",
  "i",
  "``s``s`kski",
  "```ss``ss`ki``s`ksk",
  "``ci``s``s`kski",
  "``s``s`ksk``ci``s``s`kski",
  "```ss``ss`k``ci``s``s`kski``s`ksk",
  "```ss``ss``ss`k``ci``s``s`kski``s`ksk",
  "```s``s`ksk`ci``s``s`kski",
  "````ss`ss``ss`ki``s`ksk",
  "```ss```ss`ss``ss`ki``s`ksk", // 10
  "```ss``ss```ss`ss``ss`ki``s`ksk",
  "```ss``ss``ss```ss`ss``ss`ki``s`ksk",
  "```ss``ss``ss``ss```ss`ss``ss`ki``s`ksk",
  "```ss``ss``ss``ss``ss```ss`ss``ss`ki``s`ksk",
  "```ss``ss``ss``ss``ss``ss```ss`ss``ss`ki``s`ksk",
  "```s`cii``s``s`kski",
  "``s``s`ksk```s`cii``s``s`kski",
  "```ss``ss`k```s`cii``s``s`kski``s`ksk",
  "```ss``ss``ss`k```s`cii``s``s`kski``s`ksk",
  "````sss``s`ksk``ci``s``s`kski", // 20
  "```ss``s``sss`k``ci``s``s`kski``s`ksk",
  "```ss``ss``s``sss`k``ci``s``s`kski``s`ksk",
  "```ss``ss``ss``s``sss`k``ci``s``s`kski``s`ksk",
  "```s``si``s`ci`k`s``s`ksk``s`cii``s``s`kski",
};

typedef enum {
  UNL_PC = 0, UNL_A, UNL_B, UNL_C, UNL_D, UNL_BP, UNL_SP
} UnlReg;

static UnlReg unl_regpos(Reg r) {
  switch (r) {
  case A: return UNL_A;
  case B: return UNL_B;
  case C: return UNL_C;
  case D: return UNL_D;
  case BP: return UNL_BP;
  case SP: return UNL_SP;
  default:
    error("unknown reg: %d", r);
  }
}

static void unl_emit(const char* s) {
  fputs(s, stdout);
}

static void unl_emit_tick(int n) {
  for (int i = 0; i < n; i++) {
    putchar('`');
  }
}

static void unl_list_begin(int is_first) {
  if (!is_first)
    unl_emit(K1);
  unl_emit("``s``si`k");
}

static void unl_list_end(void) {
  putchar('v');
}

static void unl_emit_churchnum(int n) {
  if (n > 24)
    error("churchnum table out of range: %d", n);
  unl_emit(CHURCHNUM[n]);
}

static void unl_emit_number(int n) {
  for (int i = 0; i < 24; i++) {
    unl_list_begin(i == 0);
    unl_emit(n % 2 ? "k" : "`ki");
    n /= 2;
  }
  unl_list_end();
}

// Compact, but less efficient
static void unl_emit_number2(int n) {
  int all_one = UINT_MAX;
  for (int i = 0; i < 24; i++) {
    if (i < 23 && n == 0) {
      if (i > 0) {
	unl_emit(K1);
      }
      unl_emit_tick(2);
      unl_emit_churchnum(24 - i);
      unl_emit(CONS_KI);
      break;
    } else if (i < 23 && n == all_one) {
      if (i > 0) {
	unl_emit(K1);
      }
      unl_emit_tick(2);
      unl_emit_churchnum(24 - i);
      unl_emit(CONS_K);
      break;
    } else {
      unl_list_begin(i == 0);
      unl_emit(n % 2 ? "k" : "`ki");
      n /= 2;
      all_one /= 2;
    }
  }
  unl_list_end();
}

static void unl_nth_begin(int n) {
  unl_emit(S2);
  unl_emit(S2);
  unl_emit(K1);
  unl_emit("`");
  unl_emit_churchnum(n);
  unl_emit(CDR);
}

static void unl_nth_end(void) {
  unl_emit(K1);
  unl_emit(DOTCAR);
}

typedef enum {
  LIB_ADD = 2,
  LIB_SUB,
  LIB_EQ,
  LIB_LT,
  LIB_LOAD,
  LIB_STORE,
  LIB_PUTC,
  LIB_GETC
} LibNo;

static void unl_emit_lib(LibNo n) {
  unl_emit(S2);
  unl_emit("`");
  unl_emit_churchnum(n);
  unl_emit(CDR);
  unl_emit(K1);
  unl_emit(DOTCAR);
}

static void unl_lib_add() {
  unl_emit(S2);
  unl_emit(S2);
  unl_emit_lib(LIB_ADD);
}

static void unl_lib_sub() {
  unl_emit(S2);
  unl_emit(S2);
  unl_emit_lib(LIB_SUB);
}

static void unl_lib_eq() {
  unl_emit(S2);
  unl_emit(S2);
  unl_emit_lib(LIB_EQ);
}

static void unl_lib_lt() {
  unl_emit(S2);
  unl_emit(S2);
  unl_emit_lib(LIB_LT);
}

static void unl_lib_load() {
  unl_emit(S2);

  unl_emit(S2);
  unl_emit_lib(LIB_LOAD);
  putchar('i');
}

static void unl_lib_store() {
  unl_emit(S2);
  unl_emit(S2);

  unl_emit(S2);
  unl_emit_lib(LIB_STORE);
  putchar('i');
}

static void unl_lib_putc() {
  unl_emit(S2);
  unl_emit_lib(LIB_PUTC);
}

static void unl_lib_getc() {
  unl_emit(S2);
  unl_emit("`kc");
  unl_emit_lib(LIB_GETC);
}

static void unl_getreg(Reg reg) {
  unl_nth_begin(unl_regpos(reg));
  unl_emit(CAR);  // regs = car(vm)
  unl_nth_end();
}

static void unl_emit_value(Value* val) {
  if (val->type == REG) {
    unl_getreg(val->reg);
  } else {
    unl_emit(K1);
    unl_emit_number(val->imm);
  }
}

static void unl_setreg_begin(UnlReg reg) {
  unl_emit(S2);
  unl_emit(S2);
  unl_emit(K1);
  unl_emit(CONS);
  unl_emit(S2);

  unl_emit(S2);
  unl_emit(K1);
  unl_emit_tick(1);
  unl_emit_churchnum(reg);
  unl_emit(APPLY_CDR);
  unl_emit(S2);
  unl_emit(K1);
  unl_emit(REPLACE_CAR);
}

static void unl_setreg_end(void) {
  unl_emit(CAR);
  unl_emit(CDR);
}

static void unl_setreg_imm(UnlReg reg, int n) {
  unl_emit(S2);
  unl_emit(S2);
  unl_emit(K1);
  unl_emit(CONS);
  unl_emit(S2);

  unl_emit(K1);
  unl_emit_tick(2);
  unl_emit_churchnum(reg);
  unl_emit(APPLY_CDR);
  unl_emit(REPLACE_CAR1);
  unl_emit_number(n);

  unl_emit(CAR);
  unl_emit(CDR);
}

static void unl_emit_jmp(Value* jmp) {
  if (jmp->type == REG) {
    unl_setreg_begin(UNL_PC);
    unl_getreg(jmp->reg);
    unl_setreg_end();
  } else {
    unl_setreg_imm(UNL_PC, jmp->imm);
  }
}

static void unl_bool_to_number_begin(void) {
  unl_emit(S2);
  unl_emit(S2);
  unl_emit(K1);
  unl_emit(CONS);
}

static void unl_bool_to_number_end(void) {
  unl_emit(K1);
  unl_emit_tick(2);
  unl_emit_churchnum(23);
  unl_emit(CONS_KI);
  putchar('v');
}

static void unl_emit_op(Inst* inst) {
  switch (inst->op) {
  case MOV:
    if (inst->src.type == REG) {
      unl_setreg_begin(unl_regpos(inst->dst.reg));
      unl_getreg(inst->src.reg);
      unl_setreg_end();
    } else {
      unl_setreg_imm(unl_regpos(inst->dst.reg), inst->src.imm);
    }
    break;

  case ADD:
    unl_setreg_begin(unl_regpos(inst->dst.reg));
    if (inst->src.type == IMM && inst->src.imm == 1) {
      unl_emit(S2);
      unl_getreg(inst->dst.reg);
      unl_emit(K1);
      unl_emit(POST_INC);
    } else if (inst->src.type == IMM && inst->src.imm == UINT_MAX) {
      unl_emit(S2);
      unl_getreg(inst->dst.reg);
      unl_emit(K1);
      unl_emit(POST_DEC);
    } else {
      unl_lib_add();
      unl_getreg(inst->dst.reg);
      unl_emit_value(&inst->src);
    }
    unl_setreg_end();
    break;

  case SUB:
    unl_setreg_begin(unl_regpos(inst->dst.reg));
    if (inst->src.type == IMM && inst->src.imm == 1) {
      unl_emit(S2);
      unl_getreg(inst->dst.reg);
      unl_emit(K1);
      unl_emit(POST_DEC);
    } else {
      unl_lib_sub();
      unl_getreg(inst->dst.reg);
      unl_emit_value(&inst->src);
    }
    unl_setreg_end();
    break;

  case JMP:
    unl_emit_jmp(&inst->jmp);
    break;

  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
    unl_emit(S2);
    unl_emit(S2);
    if (inst->op == JEQ || inst->op == JNE) {
      unl_lib_eq();
    } else {
      unl_lib_lt();
    }
    if (inst->op == JGT || inst->op == JLE) {
      unl_emit_value(&inst->src);
      unl_emit_value(&inst->dst);
    } else {
      unl_emit_value(&inst->dst);
      unl_emit_value(&inst->src);
    }
    if (inst->op == JNE || inst->op == JLE || inst->op == JGE) {
      putchar('i');
      unl_emit_jmp(&inst->jmp);
    } else {
      unl_emit_jmp(&inst->jmp);
      putchar('i');
    }
    break;

  case EQ:
  case NE:
  case LT:
  case GT:
  case LE:
  case GE:
    unl_setreg_begin(unl_regpos(inst->dst.reg));
    unl_bool_to_number_begin();
    if (inst->op == NE || inst->op == LE || inst->op == GE) {
      unl_emit(S2);
      unl_emit(S2);
    }
    if (inst->op == EQ || inst->op == NE) {
      unl_lib_eq();
    } else {
      unl_lib_lt();
    }
    if (inst->op == GT || inst->op == LE) {
      unl_emit_value(&inst->src);
      unl_emit_value(&inst->dst);
    } else {
      unl_emit_value(&inst->dst);
      unl_emit_value(&inst->src);
    }
    if (inst->op == NE || inst->op == LE || inst->op == GE) {
      unl_emit("`k`ki");
      unl_emit("`kk");
    }
    unl_bool_to_number_end();
    unl_setreg_end();
    break;

  case LOAD:
    unl_setreg_begin(unl_regpos(inst->dst.reg));
    unl_lib_load();
    unl_emit_value(&inst->src);
    unl_setreg_end();
    break;

  case STORE:
    unl_lib_store();
    unl_emit_value(&inst->src);
    unl_emit_value(&inst->dst);
    break;

  case PUTC:
    unl_emit(S2);
    unl_emit("k");
    unl_lib_putc();
    unl_emit_value(&inst->src);
    break;

  case GETC:
    unl_setreg_begin(unl_regpos(inst->dst.reg));
    unl_lib_getc();
    unl_setreg_end();
    break;

  case EXIT:
    unl_emit("``s`ke`ki");
    break;

  case DUMP:
    putchar('i');
    break;

  default:
    error("not implemented %d", inst->op);
  }
}

static Inst* unl_emit_chunk(Inst* inst) {
  int pc = inst->pc;
  printf("\n# pc=%d\n", pc);

  Inst* reversed = NULL;
  while (inst && inst->pc == pc) {
    Inst* next = inst->next;
    inst->next = reversed;
    reversed = inst;
    inst = next;
  }

  for (; reversed; reversed = reversed->next) {
    printf("# ");
    dump_inst_fp(reversed, stdout);

    if (reversed->next) {
      unl_emit_tick(2);
      unl_emit(COMPOSE);
    }
    unl_emit_op(reversed);
    putchar('\n');
  }

  return inst;
}

static void unl_emit_text(Inst* inst) {
  int first = 1;
  while (inst) {
    unl_list_begin(first);
    first = 0;
    inst = unl_emit_chunk(inst);
  }
  unl_list_end();
}

static void unl_emit_data(Data* data) {
  for (Data* d = data; d; d = d->next) {
    unl_list_begin(d == data);
    unl_emit_number2(d->v);
  }
  unl_list_end();
}

static void unl_emit_print(int n) {
  if (n == 10) {
    putchar('r');
  } else {
    putchar('.');
    putchar(n);
  }
}

static void unl_emit_putc_rec(int n, int b) {
  if (b == 128) {
    unl_emit("``s``s`ks``s``s`ksk`k`k");
    unl_emit_print(n + b);
    unl_emit("`k`k");
    unl_emit_print(n);
  } else {
    unl_emit("``s`k`si``s``s`ks``s``s`ksk");
    unl_emit("`k`k");
    unl_emit_putc_rec(n + b, b + b);
    unl_emit("`k`k");
    unl_emit_putc_rec(n, b + b);
  }
}

static void unl_emit_libputc(void) {
  unl_emit(S2);
  unl_emit(S2);
  putchar('i');
  unl_emit(K1);
  unl_emit_putc_rec(0, 1);
  unl_emit("`ki");
}

static void unl_emit_libgetc(void) {
  unl_emit("``s`d`@k");
  for (int c = 1; c < 256; c++) {
    unl_emit(S2);
    unl_emit(S2);
    unl_emit("`d");
    unl_emit("`?");
    putchar(c);
    putchar('i');
    unl_emit(K1);
    unl_emit_number2(c);
  }
  unl_emit(S2);
  putchar('i');
  unl_emit(K1);
  unl_emit_number2(0);
}

static void unl_emit_libs(void) {
  unl_list_begin(1);
  unl_emit_libputc();
  unl_list_begin(0);
  unl_emit_libgetc();
  unl_list_end();
}

static void unl_emit_core(void) {
  unl_emit(unl_core);
  unl_emit_libs();
  putchar('\n');
}

void target_unl(Module* module) {
  unl_emit_tick(2);
  printf("# VM core\n");
  unl_emit_core();
  printf("# instructions\n");
  unl_emit_text(module->text);
  printf("# data\n");
  unl_emit_data(module->data);
  putchar('\n');
}
