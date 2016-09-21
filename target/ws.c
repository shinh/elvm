// http://compsoc.dur.ac.uk/whitespace/tutorial.html

#include <stdbool.h>

#include <ir/ir.h>
#include <target/util.h>

static const int MEM = 8;

static int mem(int a) { return MEM + a; }

typedef enum {
  WS_PUSH,
  WS_DUP,
  WS_COPY,
  WS_SWAP,
  WS_DISCARD,
  WS_SLIDE,

  WS_ADD,
  WS_SUB,
  WS_MUL,
  WS_DIV,
  WS_MOD,

  WS_STORE,
  WS_RETRIEVE,

  WS_MARK,
  WS_CALL,
  WS_JMP,
  WS_JZ,
  WS_JN,
  WS_RET,
  WS_EXIT,

  WS_PUTC,
  WS_PUTINT,
  WS_GETC,
  WS_GETINT,
} WsOp;

static const char* WS_OPS[] = {
  "  ",
  " \n ",
  " \t ",
  " \n\t",
  " \n\n",
  " \t\n",

  "\t   ",
  "\t  \t",
  "\t  \n",
  "\t \t ",
  "\t \t\t",

  "\t\t ",
  "\t\t\t",

  "\n  ",
  "\n \t",
  "\n \n",
  "\n\t ",
  "\n\t\t",
  "\n\t\n",
  "\n\n\n",

  "\t\n  ",
  "\t\n \t",
  "\t\n\t ",
  "\t\n\t\t",
};

static void ws_emit_str(const char* s) {
  fputs(s, stdout);
}

static void ws_emit_num(int v) {
  char buf[40];
  int i = 39;
  buf[i--] = 0;
  buf[i--] = '\n';
  do {
    buf[i--] = " \t"[v % 2];
    v /= 2;
  } while (v);
  buf[i] = ' ';
  ws_emit_str(&buf[i]);
}

static void ws_emit_uint_mod_ws() {
  putchar(' ');
  putchar('\t');
  for (int i = 0; i < 24; i++)
    putchar(' ');
  putchar('\n');
}

static void ws_emit(WsOp op) {
  ws_emit_str(WS_OPS[op]);
}

static void ws_emit_op(WsOp op, int a) {
  ws_emit(op);
  ws_emit_num(a);
}

static void ws_emit_local_jmp(WsOp op, int l) {
  ws_emit(op);
  ws_emit_num(l);
}

static void ws_emit_store(int addr, int val) {
  ws_emit_op(WS_PUSH, addr);
  ws_emit_op(WS_PUSH, val);
  ws_emit(WS_STORE);
}

static void ws_emit_retrieve(int addr) {
  ws_emit_op(WS_PUSH, addr);
  ws_emit(WS_RETRIEVE);
}

static void ws_emit_value(Value* v, int off) {
  if (v->type == REG) {
    ws_emit_retrieve(v->reg);
    if (off) {
      ws_emit_op(WS_PUSH, off);
      ws_emit(WS_ADD);
    }
  } else {
    ws_emit_op(WS_PUSH, v->imm + off);
  }
}

static void ws_emit_src(Inst* inst, int off) {
  ws_emit_value(&inst->src, off);
}

static void ws_emit_addsub(Inst* inst, WsOp op) {
  ws_emit_op(WS_PUSH, inst->dst.reg);
  ws_emit_retrieve(inst->dst.reg);
  ws_emit_src(inst, 0);
  ws_emit(op);
  ws_emit(WS_PUSH); ws_emit_uint_mod_ws();
  ws_emit(WS_ADD);
  ws_emit(WS_PUSH); ws_emit_uint_mod_ws();
  ws_emit(WS_MOD);
  ws_emit(WS_STORE);
}

static void ws_emit_cmp_ws(Inst* inst, int flip, int* label) {
  int lf = ++*label;
  int lt = ++*label;
  int ld = ++*label;
  int op = normalize_cond(inst->op, flip);

  if (op == JGT || op == JLE) {
    op = op == JGT ? JLT : JGE;
    ws_emit_src(inst, 0);
    ws_emit_retrieve(inst->dst.reg);
  } else {
    ws_emit_retrieve(inst->dst.reg);
    ws_emit_src(inst, 0);
  }
  ws_emit(WS_SUB);

  switch (op) {
    case JEQ:
      ws_emit_local_jmp(WS_JZ, lt);
      ws_emit_local_jmp(WS_JMP, lf);
      break;

    case JNE:
      ws_emit_local_jmp(WS_JZ, lf);
      ws_emit_local_jmp(WS_JMP, lt);
      break;

    case JLT:
      ws_emit_local_jmp(WS_JN, lt);
      ws_emit_local_jmp(WS_JMP, lf);
      break;

    case JGE:
      ws_emit_local_jmp(WS_JN, lf);
      ws_emit_local_jmp(WS_JMP, lt);
      break;

    default:
      error("oops %d", op);
  }

  ws_emit_op(WS_MARK, lt);
  ws_emit_op(WS_PUSH, 1);
  ws_emit_op(WS_JMP, ld);
  ws_emit_op(WS_MARK, lf);
  ws_emit_op(WS_PUSH, 0);
  ws_emit_op(WS_MARK, ld);
}

static void ws_emit_jmp(Inst* inst, WsOp op, int reg_jmp) {
  if (inst->jmp.type == REG) {
    ws_emit_retrieve(inst->jmp.reg);
    if (op != WS_JMP) {
      ws_emit(WS_SWAP);
    }
    ws_emit_op(op, reg_jmp);
  } else {
    ws_emit_op(op, inst->jmp.imm);
  }
}

static void ws_emit_reg_jmp_table(int min_pc, int max_pc, int last_label) {
  if (min_pc + 1 == max_pc) {
    ws_emit(WS_DISCARD);
    ws_emit_op(WS_JMP, min_pc);
    return;
  }

  int mid_pc = (min_pc + max_pc) / 2;
  ws_emit(WS_DUP);
  ws_emit_op(WS_PUSH, mid_pc);
  ws_emit(WS_SUB);
  ws_emit_op(WS_JN, last_label + mid_pc);
  ws_emit_reg_jmp_table(mid_pc, max_pc, last_label);
  ws_emit_op(WS_MARK, last_label + mid_pc);
  ws_emit_reg_jmp_table(min_pc, mid_pc, last_label);
}

static void init_state_ws(Data* data) {
  for (int i = 0; i < 7; i++) {
    ws_emit_store(i, 0);
  }
  for (int mp = 0; data; data = data->next, mp++) {
    ws_emit_store(mem(mp), data->v);
  }
}

void target_ws(Module* module) {
  init_state_ws(module->data);

  int label = 0;
  for (Inst* inst = module->text; inst; inst = inst->next) {
    label = inst->pc;
  }
  int reg_jmp = ++label;

  int prev_pc = -1;
  for (Inst* inst = module->text; inst; inst = inst->next) {
    if (prev_pc != inst->pc) {
      ws_emit_op(WS_MARK, inst->pc);
    }
    prev_pc = inst->pc;

    switch (inst->op) {
      case MOV:
        ws_emit_op(WS_PUSH, inst->dst.reg);
        ws_emit_src(inst, 0);
        ws_emit(WS_STORE);
        break;

      case ADD:
        ws_emit_addsub(inst, WS_ADD);
        break;

      case SUB:
        ws_emit_addsub(inst, WS_SUB);
        break;

      case LOAD:
        ws_emit_op(WS_PUSH, inst->dst.reg);
        ws_emit_src(inst, 8);
        ws_emit(WS_RETRIEVE);
        ws_emit(WS_STORE);
        break;

      case STORE:
        ws_emit_src(inst, 8);
        ws_emit_retrieve(inst->dst.reg);
        ws_emit(WS_STORE);
        break;

      case PUTC:
        ws_emit_src(inst, 0);
        ws_emit(WS_PUTC);
        break;

      case GETC:
        ws_emit_op(WS_PUSH, inst->dst.reg);
        ws_emit(WS_GETC);
        break;

      case EXIT:
        ws_emit(WS_EXIT);
        break;

      case DUMP:
        break;

      case EQ:
      case NE:
      case LT:
      case GT:
      case LE:
      case GE:
        ws_emit_op(WS_PUSH, inst->dst.reg);
        ws_emit_cmp_ws(inst, 0, &label);
        ws_emit(WS_STORE);
        break;

      case JEQ:
      case JNE:
      case JLT:
      case JGT:
      case JLE:
      case JGE:
        ws_emit_cmp_ws(inst, 1, &label);
        ws_emit_jmp(inst, WS_JZ, reg_jmp);
        break;

      case JMP:
        ws_emit_jmp(inst, WS_JMP, reg_jmp);
        break;

      default:
        error("oops");
    }
  }

  ws_emit_op(WS_MARK, reg_jmp);
  ws_emit_reg_jmp_table(0, reg_jmp, label);
}
