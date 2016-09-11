#ifndef ELVM_UTIL_H_
#define ELVM_UTIL_H_

char* format(const char* fmt, ...);

__attribute__((noreturn))
void error(const char* fmt, ...);

void inc_indent();
void dec_indent();
void emit_line(const char* fmt, ...);

#endif  // ELVM_UTIL_H_
