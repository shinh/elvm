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

static void emit_str(const char* s) {
  fputs(s, stdout);
}

static void emit_num(int v) {
  char buf[40];
  int i = 39;
  buf[i--] = 0;
  buf[i--] = '\n';
  do {
    buf[i--] = " \t"[v % 2];
    v /= 2;
  } while (v);
  buf[i] = ' ';
  emit_str(&buf[i]);
}

static void emit(WsOp op) {
  emit_str(WS_OPS[op]);
}

static void emit_op(WsOp op, int a) {
  emit(op);
  emit_num(a);
}

static void emit_local_jmp(WsOp op, int l) {
  emit(op);
  emit_num(l);
}

static void emit_store(int addr, int val) {
  emit_op(WS_PUSH, addr);
  emit_op(WS_PUSH, val);
  emit(WS_STORE);
}

static void emit_retrieve(int addr) {
  emit_op(WS_PUSH, addr);
  emit(WS_RETRIEVE);
}

static void emit_value(Value* v, int off) {
  if (v->type == REG) {
    emit_retrieve(v->reg);
    if (off) {
      emit_op(WS_PUSH, off);
      emit(WS_ADD);
    }
  } else {
    emit_op(WS_PUSH, v->imm + off);
  }
}

static void emit_src(Inst* inst, int off) {
  emit_value(&inst->src, off);
}

static void emit_addsub(Inst* inst, WsOp op) {
  emit_op(WS_PUSH, inst->dst.reg);
  emit_retrieve(inst->dst.reg);
  emit_src(inst, 0);
  emit(op);
  emit_op(WS_PUSH, 1 << 24);
  emit(WS_ADD);
  emit_op(WS_PUSH, 1 << 24);
  emit(WS_MOD);
  emit(WS_STORE);
}

static void emit_cmp(Inst* inst, int flip, int* label) {
  int lf = ++*label;
  int lt = ++*label;
  int ld = ++*label;
  int op = normalize_cond(inst->op, flip);

  if (op == JGT || op == JLE) {
    op = op == JGT ? JLT : JGE;
    emit_src(inst, 0);
    emit_retrieve(inst->dst.reg);
  } else {
    emit_retrieve(inst->dst.reg);
    emit_src(inst, 0);
  }
  emit(WS_SUB);

  switch (op) {
    case JEQ:
      emit_local_jmp(WS_JZ, lt);
      emit_local_jmp(WS_JMP, lf);
      break;

    case JNE:
      emit_local_jmp(WS_JZ, lf);
      emit_local_jmp(WS_JMP, lt);
      break;

    case JLT:
      emit_local_jmp(WS_JN, lt);
      emit_local_jmp(WS_JMP, lf);
      break;

    case JGE:
      emit_local_jmp(WS_JN, lf);
      emit_local_jmp(WS_JMP, lt);
      break;

    default:
      error("oops %d", op);
  }

  emit_op(WS_MARK, lt);
  emit_op(WS_PUSH, 1);
  emit_op(WS_JMP, ld);
  emit_op(WS_MARK, lf);
  emit_op(WS_PUSH, 0);
  emit_op(WS_MARK, ld);
}

static void init_state(Data* data) {
  for (int i = 0; i < 7; i++) {
    emit_store(i, 0);
  }
  for (int mp = 0; data; data = data->next, mp++) {
    emit_store(mem(mp), data->v);
  }
}

void target_ws(Module* module) {
  init_state(module->data);

  int label = 0;
  for (Inst* inst = module->text; inst; inst = inst->next) {
    label = inst->pc;
  }

  int prev_pc = -1;
  for (Inst* inst = module->text; inst; inst = inst->next) {
    if (prev_pc != inst->pc) {
      emit_op(WS_MARK, inst->pc);
    }
    prev_pc = inst->pc;

    switch (inst->op) {
      case MOV:
        emit_op(WS_PUSH, inst->dst.reg);
        emit_src(inst, 0);
        emit(WS_STORE);
        break;

      case ADD:
        emit_addsub(inst, WS_ADD);
        break;

      case SUB:
        emit_addsub(inst, WS_SUB);
        break;

      case LOAD:
        emit_op(WS_PUSH, inst->dst.reg);
        emit_src(inst, 8);
        emit(WS_RETRIEVE);
        emit(WS_STORE);
        break;

      case STORE:
        emit_src(inst, 8);
        emit_retrieve(inst->dst.reg);
        emit(WS_STORE);
        break;

      case PUTC:
        emit_src(inst, 0);
        emit(WS_PUTC);
        break;

      case GETC:
        emit_op(WS_PUSH, inst->dst.reg);
        emit(WS_GETC);
        break;

      case EXIT:
        emit(WS_EXIT);
        break;

      case DUMP:
        break;

      case EQ:
      case NE:
      case LT:
      case GT:
      case LE:
      case GE:
        emit_retrieve(inst->dst.reg);
        emit_cmp(inst, 0, &label);
        emit(WS_STORE);
        break;

      case JEQ:
      case JNE:
      case JLT:
      case JGT:
      case JLE:
      case JGE:
        emit_cmp(inst, 1, &label);
        emit_op(WS_JZ, inst->jmp.imm);
        break;

      case JMP:
        emit_op(WS_JMP, inst->jmp.imm);
        break;

      default:
        error("oops");
    }
  }
}
