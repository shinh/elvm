#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ir/ir.h>
#include <target/util.h>

static const char* REG_NAMES[] = {
  "a", "b", "c", "d", "bp", "sp"
};

static void init_state(Data* data) {
  printf("pc = 0\n");
  for (int i = 0; i < 6; i++) {
    printf("%s = 0\n", REG_NAMES[i]);
  }
  printf("mem = [0] * (1 << 24)\n");
  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      printf("mem[%d] = %d\n", mp, data->v);
    }
  }
}

static const char* value(Value* v) {
  if (v->type == REG) {
    return REG_NAMES[v->reg];
  } else if (v->type == IMM) {
    return format("%d", v->imm);
  } else {
    error("invalid value");
  }
}

static const char* src(Inst* inst) {
  return value(&inst->src);
}

static const char* cmp(Inst* inst) {
  int op = inst->op;
  if (op >= 16)
    op -= 8;
  const char* op_str;
  switch (op) {
    case JEQ:
      op_str = "=="; break;
    case JNE:
      op_str = "!="; break;
    case JLT:
      op_str = "<"; break;
    case JGT:
      op_str = ">"; break;
    case JLE:
      op_str = "<="; break;
    case JGE:
      op_str = ">="; break;
    case JMP:
      return "true";
    default:
      error("oops");
  }
  return format("%s %s %s", REG_NAMES[inst->dst.reg], op_str, src(inst));
}

void target_rb(Module* module) {
  init_state(module->data);

  printf("while true\n");
  printf("case pc\n");

  int prev_pc = -1;
  for (Inst* inst = module->text; inst; inst = inst->next) {
    if (prev_pc != inst->pc) {
      printf("\n");
      printf("when %d\n", inst->pc);
    }
    prev_pc = inst->pc;

    switch (inst->op) {
      case MOV:
        printf("%s = %s\n", REG_NAMES[inst->dst.reg], src(inst));
        break;

      case ADD:
        printf("%s = (%s + %s) & " UINT_MAX_STR "\n",
               REG_NAMES[inst->dst.reg], REG_NAMES[inst->dst.reg], src(inst));
        break;

      case SUB:
        printf("%s = (%s - %s) & " UINT_MAX_STR "\n",
               REG_NAMES[inst->dst.reg], REG_NAMES[inst->dst.reg], src(inst));
        break;

      case LOAD:
        printf("%s = mem[%s]\n", REG_NAMES[inst->dst.reg], src(inst));
        break;

      case STORE:
        printf("mem[%s] = %s\n", src(inst), REG_NAMES[inst->dst.reg]);
        break;

      case PUTC:
        printf("putc %s\n", src(inst));
        break;

      case GETC:
        printf("c = STDIN.getc; %s = c ? c.ord : 0\n",
               REG_NAMES[inst->dst.reg]);
        break;

      case EXIT:
        printf("exit\n");
        break;

      case DUMP:
        break;

      case EQ:
      case NE:
      case LT:
      case GT:
      case LE:
      case GE:
        printf("%s = %s ? 1 : 0\n", REG_NAMES[inst->dst.reg], cmp(inst));
        break;

      case JEQ:
      case JNE:
      case JLT:
      case JGT:
      case JLE:
      case JGE:
      case JMP:
        if (cmp(inst)) {
          printf("%s && pc = %s - 1\n", cmp(inst), value(&inst->jmp));
        }
        break;

      default:
        error("oops");
    }
  }

  printf("end\n");
  printf("pc += 1\n");
  printf("end\n");
}
