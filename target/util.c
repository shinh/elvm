#include <target/util.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

char* vformat(const char* fmt, va_list ap) {
  char buf[256];
  va_list va_cpy;
  va_copy(va_cpy, ap);
  int n = vsnprintf(buf, 256, fmt, ap) + 1;
  if (n > 256) {
    char* buf2 = (char*) malloc(n * sizeof(char));
    vsnprintf(buf2, n, fmt, va_cpy);
    return buf2;
  }
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
static int g_emit_cnt = 0;
static bool g_emit_started = true;

void inc_indent() {
  g_indent++;
}

void dec_indent() {
  g_indent--;
}

void emit_line(const char* fmt, ...) {
  if (fmt[0]) {
    if (g_emit_started){
      g_emit_cnt += g_indent;
      for (int i = 0; i < g_indent; i++)
        putchar(' ');
      va_list ap;
      va_start(ap, fmt);
      g_emit_cnt += vprintf(fmt, ap);
      va_end(ap);
    } else {
      g_emit_cnt += g_indent;
      va_list ap;
      va_start(ap, fmt);
      g_emit_cnt += vsnprintf(NULL, 0, fmt, ap);
      va_end(ap);
    }
  }
  if (g_emit_started)
    putchar('\n');
}

void emit_str(const char* fmt, ...) {
  if (fmt[0]) {
    if (g_emit_started){
      g_emit_cnt += g_indent;
      for (int i = 0; i < g_indent; i++)
        putchar(' ');
      va_list ap;
      va_start(ap, fmt);
      g_emit_cnt += vprintf(fmt, ap);
      va_end(ap);
    } else {
      g_emit_cnt += g_indent;
      va_list ap;
      va_start(ap, fmt);
      g_emit_cnt += vsnprintf(NULL, 0, fmt, ap);
      va_end(ap);
    }
  }
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

Op normalize_cond(Op op, bool flip) {
  if (op >= 16)
    op -= 8;
  if (flip) {
    static const Op TBL[] = {
      JNE, JEQ, JGE, JLE, JGT, JLT, JMP
    };
    op = TBL[op-JEQ];
  }
  return (Op)op;
}

const char* cmp_str(Inst* inst, const char* true_str) {
  int op = normalize_cond(inst->op, 0);
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

int emit_cnt() {
  return g_emit_cnt;
}

void emit_reset() {
  g_emit_cnt = 0;
  g_emit_started = false;
}

void emit_start() {
  g_emit_started = true;
}

void emit_1(int a) {
  g_emit_cnt++;
  if (g_emit_started)
    putchar(a);
}

void emit_2(int a, int b) {
  emit_1(a);
  emit_1(b);
}

void emit_3(int a, int b, int c) {
  emit_1(a);
  emit_1(b);
  emit_1(c);
}

void emit_4(int a, int b, int c, int d) {
  emit_1(a);
  emit_1(b);
  emit_1(c);
  emit_1(d);
}

void emit_5(int a, int b, int c, int d, int e) {
  emit_1(a);
  emit_1(b);
  emit_1(c);
  emit_1(d);
  emit_1(e);
}

void emit_6(int a, int b, int c, int d, int e, int f) {
  emit_1(a);
  emit_1(b);
  emit_1(c);
  emit_1(d);
  emit_1(e);
  emit_1(f);
}

void emit_le(uint32_t a) {
  emit_1(a % 256);
  a /= 256;
  emit_1(a % 256);
  a /= 256;
  emit_1(a % 256);
  a /= 256;
  emit_1(a);
}

void emit_diff(uint32_t a, uint32_t b) {
  uint32_t v = a - b;
  emit_1(v % 256);
  v /= 256;
  emit_1(v % 256);
  v /= 256;
  emit_1(v % 256);
  emit_1(a >= b ? 0 : 0xff);
}

int CHUNKED_FUNC_SIZE = 512;

int emit_chunked_main_loop(Inst* inst,
                           void (*emit_func_prologue)(int func_id),
                           void (*emit_func_epilogue)(void),
                           void (*emit_pc_change)(int pc),
                           void (*emit_inst)(Inst* inst)) {
  int prev_pc = -1;
  int prev_func_id = -1;
  for (; inst; inst = inst->next) {
    int func_id = inst->pc / CHUNKED_FUNC_SIZE;
    if (prev_pc != inst->pc) {
      if (prev_func_id != func_id) {
        if (prev_func_id != -1) {
          emit_func_epilogue();
        }
        emit_func_prologue(func_id);
      }

      emit_pc_change(inst->pc);
    }
    prev_pc = inst->pc;
    prev_func_id = func_id;

    emit_inst(inst);
  }
  emit_func_epilogue();
  return prev_func_id + 1;
}

#define PACK2(x) ((x) % 256), ((x) / 256)
#define PACK4(x) ((x) % 256), ((x) / 256 % 256), ((x) / 65536), 0

void emit_elf_header(uint16_t machine, uint32_t filesz) {
  const char ehdr[52] = {
    // e_ident
    0x7f, 0x45, 0x4c, 0x46, 0x01, 0x01, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    PACK2(2),  // e_type
    PACK2(machine),  // e_machine
    PACK4(1),  // e_version
    PACK4(ELF_TEXT_START + ELF_HEADER_SIZE),  // e_entry
    PACK4(52),  // e_phoff
    PACK4(0),  // e_shoff
    PACK4(0),  // e_flags
    PACK2(52),  // e_ehsize
    PACK2(32),  // e_phentsize
    PACK2(1),  // e_phnum
    PACK2(40),  // e_shentsize
    PACK2(0),  // e_shnum
    PACK2(0),  // e_shstrndx
  };
  const char phdr[32] = {
    PACK4(1),  // p_type
    PACK4(0),  // p_offset
    PACK4(ELF_TEXT_START),  // p_vaddr
    PACK4(ELF_TEXT_START),  // p_paddr
    PACK4(filesz + ELF_HEADER_SIZE),  // p_filesz
    PACK4(filesz + ELF_HEADER_SIZE),  // p_memsz
    PACK4(5),  // p_flags
    PACK4(0x1000),  // p_align
  };
  fwrite(ehdr, 52, 1, stdout);
  fwrite(phdr, 32, 1, stdout);
}

bool parse_bool_value(const char* value) {
  return *value == '1' || *value == 't' || *value == 'T';
}

bool handle_chunked_func_size_arg(const char* key, const char* value) {
  if (!strcmp(key, "chunked_func_size")) {
    CHUNKED_FUNC_SIZE = atoi(value);
    return true;
  }
  return false;
}
