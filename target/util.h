#ifndef ELVM_UTIL_H_
#define ELVM_UTIL_H_

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#include <ir/ir.h>

typedef uint32_t uint;
typedef uint8_t byte;

static const int ELF_TEXT_START = 0x100000;
static const int ELF_HEADER_SIZE = 84;

char* vformat(const char* fmt, va_list ap);
char* format(const char* fmt, ...);

#ifdef __GNUC__
__attribute__((noreturn))
#endif
void error(const char* fmt, ...);

void inc_indent();
void dec_indent();
void emit_line(const char* fmt, ...);
void emit_str(const char* fmt, ...);

Op normalize_cond(Op op, bool flip);
extern const char** reg_names;
const char* value_str(Value* v);
const char* src_str(Inst* inst);
const char* cmp_str(Inst* inst, const char* true_str);

int emit_cnt();
void emit_reset();
void emit_start();
void emit_1(int a);
void emit_2(int a, int b);
void emit_3(int a, int b, int c);
void emit_4(int a, int b, int c, int d);
void emit_5(int a, int b, int c, int d, int e);
void emit_6(int a, int b, int c, int d, int e, int f);
void emit_le(uint32_t a);
void emit_diff(uint32_t a, uint32_t b);

extern int CHUNKED_FUNC_SIZE;

int emit_chunked_main_loop(Inst* inst,
                           void (*emit_func_prologue)(int func_id),
                           void (*emit_func_epilogue)(void),
                           void (*emit_pc_change)(int pc),
                           void (*emit_inst)(Inst* inst));

void emit_elf_header(uint16_t machine, uint32_t filesz);

bool parse_bool_value(const char* value);
bool handle_chunked_func_size_arg(const char* key, const char* value);

#endif  // ELVM_UTIL_H_
