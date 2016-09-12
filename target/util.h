#ifndef ELVM_UTIL_H_
#define ELVM_UTIL_H_

#include <ir/ir.h>

char* format(const char* fmt, ...);

__attribute__((noreturn))
void error(const char* fmt, ...);

void inc_indent();
void dec_indent();
void emit_line(const char* fmt, ...);

const char** reg_names;
const char* value_str(Value* v);
const char* src_str(Inst* inst);
const char* cmp_str(Inst* inst, const char* true_str);

#endif  // ELVM_UTIL_H_
