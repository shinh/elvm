#include "util.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* vformat(const char* fmt, va_list ap) {
  char buf[256];
  vsnprintf(buf, 255, fmt, ap);
  buf[255] = 0;
  return strdup(buf);
}

char* format(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  char* r = vformat(fmt, ap);
  va_end(ap);
  return r;
}

void error(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  char* r = vformat(fmt, ap);
  va_end(ap);
  fprintf(stderr, "%s\n", r);
  exit(1);
}

static int g_indent;

void inc_indent() {
  g_indent++;
}

void dec_indent() {
  g_indent--;
}

void emit_line(const char* fmt, ...) {
  if (fmt[0]) {
    for (int i = 0; i < g_indent; i++)
      putchar(' ');
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
  }
  putchar('\n');
}

static const char* DEFAULT_REG_NAMES[7] = {
  "a", "b", "c", "d", "bp", "sp", "pc"
};

const char** reg_names = DEFAULT_REG_NAMES;

const char* value_str(Value* v) {
  if (v->type == REG) {
    return reg_names[v->reg];
  } else if (v->type == IMM) {
    return format("%d", v->imm);
  } else {
    error("invalid value");
  }
}

const char* src_str(Inst* inst) {
  return value_str(&inst->src);
}

const char* cmp_str(Inst* inst, const char* true_str) {
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
      return true_str;
    default:
      error("oops");
  }
  return format("%s %s %s", reg_names[inst->dst.reg], op_str, src_str(inst));
}
