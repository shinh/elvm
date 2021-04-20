#include <ir/ir.h>
#include <target/util.h>

//=============================================================
// Configurations
//=============================================================

#define QFTASM_RAM_AS_STDIN_BUFFER
#define QFTASM_RAM_AS_STDOUT_BUFFER

#define QFTASM_JMPTABLE_IN_ROM

#ifdef QFTASM_RAM_AS_STDIN_BUFFER
static const int QFTASM_RAMSTDIN_BUF_STARTPOSITION = 7167;
#endif

#ifdef QFTASM_RAM_AS_STDOUT_BUFFER
static const int QFTASM_RAMSTDOUT_BUF_STARTPOSITION = 8191;
#endif

// RAM pointer offset to prevent negative-value RAM addresses
// from underflowing into the register regions
static const int QFTASM_MEM_OFFSET = 2048;

// This is required to run tests on ELVM,
// since the Makefile compiles all files at first
#define QFTASM_SUPPRESS_MEMORY_INIT_OVERFLOW_ERROR

//=============================================================
static const int QFTASM_PC = 0;
static const int QFTASM_STDIN = 1;
static const int QFTASM_STDOUT = 2;
static const int QFTASM_A = 3;
static const int QFTASM_B = 4;
static const int QFTASM_C = 5;
static const int QFTASM_D = 6;
static const int QFTASM_BP = 7;
static const int QFTASM_SP = 8;
static const int QFTASM_TEMP = 9;
static const int QFTASM_TEMP_2 = 10;

#ifdef QFTASM_JMPTABLE_IN_ROM
static const int QFTASM_JMPTABLE_OFFSET = 4;
#else
static const int QFTASM_JMPTABLE_OFFSET = 11;
#endif

#define QFTASM_STDIO_OPEN (1 << 8)
#define QFTASM_STDIO_CLOSED (1 << 9)

static void qftasm_emit_line(const char* fmt, ...) {
  static int qftasm_lineno_counter_ = 0;
  printf("%d. ", qftasm_lineno_counter_);
  qftasm_lineno_counter_++;

  if (fmt[0]) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
  }
  putchar('\n');
}

static int qftasm_int24_to_int16(int x) {
  // Interpret x as a 24-bit signed integer.
  // If it is negative, then reinterpret x as a 16-bit signed integer.
  if (x < 0) {
    x += (1 << 16);
  } else if (x > (1 << 23)) {
    x = x - (1 << 24) + (1 << 16);
  }
  x = x & ((1 << 16) - 1);
  return x;
}

static int get_max_pc(Inst* inst) {
  int max_pc = 0;
  for (; inst; inst = inst->next) {
    max_pc = inst->pc > max_pc ? inst->pc : max_pc;
  }
  return max_pc;
}

static void qftasm_emit_jump_table(Inst* init_inst) {
  int max_pc = get_max_pc(init_inst);

#ifdef QFTASM_JMPTABLE_IN_ROM
  qftasm_emit_line("MNZ 1 %d %d; Initially skip the jump table", QFTASM_JMPTABLE_OFFSET + (max_pc+1)*2 - 1, QFTASM_PC);
  qftasm_emit_line("MNZ 0 0 0;");
  for (int pc = 0; pc <= max_pc; pc++){
    qftasm_emit_line("MNZ 1 {pc%d} %d;%s", pc, QFTASM_PC,
                     pc == 0 ? " Jump table" : "");
    qftasm_emit_line("MNZ 0 0 0");
  }
#else
  for (int pc = 0; pc <= max_pc; pc++){
    qftasm_emit_line("MNZ 1 {pc%d} %d;%s", pc, pc + QFTASM_JMPTABLE_OFFSET,
                     pc == 0 ? " Jump table" : "");
  }
#endif
}

static void qftasm_emit_memory_initialization(Data* data) {
  // RAM initialization
  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
#ifndef QFTASM_SUPPRESS_MEMORY_INIT_OVERFLOW_ERROR
      if (mp + QFTASM_MEM_OFFSET>= (1 << 16)) {
        error("Memory pointer overflow occured at memory initialization: Address %d", mp + QFTASM_MEM_OFFSET);
      }
#endif
      qftasm_emit_line("MNZ 1 %d %d;%s",
                       qftasm_int24_to_int16(data->v), mp + QFTASM_MEM_OFFSET,
                       mp == 0 ? " Memory table" : "");
    }
  }
}

static void init_state_qftasm(Data* data, Inst* init_inst) {
  // stdin, stdout
#ifdef QFTASM_RAM_AS_STDIN_BUFFER
  qftasm_emit_line("MNZ 1 %d %d; Register initialization (stdin buffer pointer)", QFTASM_RAMSTDIN_BUF_STARTPOSITION, QFTASM_STDIN);
#else
  qftasm_emit_line("MNZ 1 %d %d; Register initialization (stdin)", QFTASM_STDIO_CLOSED, QFTASM_STDIN);
#endif
#ifdef QFTASM_RAM_AS_STDOUT_BUFFER
  qftasm_emit_line("MNZ 1 %d %d; Register initialization (stdout buffer pointer)", QFTASM_RAMSTDOUT_BUF_STARTPOSITION, QFTASM_STDOUT);
#else
  qftasm_emit_line("MNZ 1 %d %d; Register initialization (stdout)", QFTASM_STDIO_CLOSED, QFTASM_STDOUT);
#endif

  qftasm_emit_jump_table(init_inst);
  qftasm_emit_memory_initialization(data);
}

static int qftasm_reg2addr(Reg reg) {
  switch (reg) {
    case A: return QFTASM_A;
    case B: return QFTASM_B;
    case C: return QFTASM_C;
    case D: return QFTASM_D;
    case BP: return QFTASM_BP;
    case SP: return QFTASM_SP;
    default:
      error("Undefined register name in qftasm_reg2addr: %d", reg);
  }
}

static const char* qftasm_value_str(Value* v) {
  if (v->type == REG) {
    return format("A%d", qftasm_reg2addr(v->reg));
  } else if (v->type == IMM) {
    return format("%d", qftasm_int24_to_int16(v->imm));
  } else {
    error("invalid value");
  }
}

#ifdef QFTASM_JMPTABLE_IN_ROM
static void qftasm_emit_conditional_jmp_inst(Inst* inst) {
  Value* v = &inst->jmp;
  if (v->type == REG) {
    qftasm_emit_line("ADD A%d A%d %d;", qftasm_reg2addr(v->reg), qftasm_reg2addr(v->reg), QFTASM_TEMP_2);
    qftasm_emit_line("ADD A%d %d %d;", QFTASM_TEMP_2, QFTASM_JMPTABLE_OFFSET-1, QFTASM_TEMP_2);
    qftasm_emit_line("MNZ A%d A%d %d; (reg)", QFTASM_TEMP, QFTASM_TEMP_2, QFTASM_PC);
  } else if (v->type == IMM) {
    qftasm_emit_line("MNZ A%d {pc%d} %d; (imm)", QFTASM_TEMP, v->imm, QFTASM_PC);
  } else {
    error("Invalid value at conditional jump");
  }
}

static void qftasm_emit_unconditional_jmp_inst(Inst* inst) {
  Value* v = &inst->jmp;
  if (v->type == REG) {
    qftasm_emit_line("ADD A%d A%d %d;", qftasm_reg2addr(v->reg), qftasm_reg2addr(v->reg), QFTASM_TEMP_2);
    qftasm_emit_line("ADD A%d %d %d;", QFTASM_TEMP_2, QFTASM_JMPTABLE_OFFSET-1, QFTASM_PC);
  } else if (v->type == IMM) {
    qftasm_emit_line("MNZ 1 {pc%d} %d; JMP (imm)", v->imm, QFTASM_PC);
  } else {
    error("Invalid value at JMP");
  }
}
#else
static void qftasm_emit_conditional_jmp_inst(Inst* inst) {
  Value* v = &inst->jmp;
  if (v->type == REG) {
    qftasm_emit_line("ADD A%d %d %d;", qftasm_reg2addr(v->reg), QFTASM_JMPTABLE_OFFSET, QFTASM_TEMP_2);
    qftasm_emit_line("MNZ A%d B%d %d; (reg)", QFTASM_TEMP, QFTASM_TEMP_2, QFTASM_PC);
  } else if (v->type == IMM) {
    qftasm_emit_line("MNZ A%d A%d %d; (imm)", QFTASM_TEMP, v->imm + QFTASM_JMPTABLE_OFFSET, QFTASM_PC);
  } else {
    error("Invalid value at conditional jump");
  }
}

static void qftasm_emit_unconditional_jmp_inst(Inst* inst) {
  Value* v = &inst->jmp;
  if (v->type == REG) {
    qftasm_emit_line("ADD A%d %d %d; JMP (reg)", qftasm_reg2addr(v->reg), QFTASM_JMPTABLE_OFFSET, QFTASM_TEMP_2);
    qftasm_emit_line("MNZ 1 B%d %d;", QFTASM_TEMP_2, QFTASM_PC);
  } else if (v->type == IMM) {
    qftasm_emit_line("MNZ 1 A%d %d; JMP (imm)", v->imm + QFTASM_JMPTABLE_OFFSET, QFTASM_PC);
  } else {
    error("Invalid value at JMP");
  }
}
#endif

static const char* qftasm_src_str(Inst* inst) {
  return qftasm_value_str(&inst->src);
}

static const char* qftasm_dst_str(Inst* inst) {
  return qftasm_value_str(&inst->dst);
}

static void qftasm_emit_func_prologue(int func_id) {
  // Placeholder code that does nothing, to suppress compilation errors
  if (func_id) {
    return;
  }
}

static void qftasm_emit_func_epilogue(void) {
}


static void qftasm_emit_pc_change(int pc) {
  // The comments in this line are required for post-processing step in ./tools/qftasm_pp.py
  qftasm_emit_line("MNZ 0 0 0; pc == %d:", pc);
}

static void qftasm_emit_inst(Inst* inst) {
  switch (inst->op) {
  case MOV:
    qftasm_emit_line("MNZ 1 %s %d; MOV",
                     qftasm_src_str(inst), qftasm_reg2addr(inst->dst.reg));
    break;

  case ADD:
    qftasm_emit_line("ADD A%d %s %d; ADD",
                     qftasm_reg2addr(inst->dst.reg),
                     qftasm_src_str(inst),
                     qftasm_reg2addr(inst->dst.reg));
    break;

  case SUB:
    qftasm_emit_line("SUB A%d %s %d; SUB",
                     qftasm_reg2addr(inst->dst.reg),
                     qftasm_src_str(inst),
                     qftasm_reg2addr(inst->dst.reg));
    break;

  case LOAD:
    if (inst->src.type == REG) {
      qftasm_emit_line("ADD A%d %d %d; LOAD (reg)",
                       qftasm_reg2addr(inst->src.reg), QFTASM_MEM_OFFSET, QFTASM_TEMP);
      qftasm_emit_line("MNZ 1 B%d %d;",
                       QFTASM_TEMP, qftasm_reg2addr(inst->dst.reg));
    } else if (inst->src.type == IMM) {
      qftasm_emit_line("MNZ 1 A%d %d; LOAD (imm)",
                       qftasm_int24_to_int16(inst->src.imm) + QFTASM_MEM_OFFSET, qftasm_reg2addr(inst->dst.reg));
    } else {
      error("Invalid value at LOAD");
    }
    break;

  case STORE:
    // Here, "src" and "dst" have opposite meanings from their names
    if (inst->src.type == REG) {
      qftasm_emit_line("ADD A%d %d %d; STORE (reg)",
                       qftasm_reg2addr(inst->src.reg), QFTASM_MEM_OFFSET, QFTASM_TEMP);
      qftasm_emit_line("MNZ 1 A%d A%d;",
                       qftasm_reg2addr(inst->dst.reg), QFTASM_TEMP);
    } else if (inst->src.type == IMM) {
      qftasm_emit_line("MNZ 1 A%d %d; STORE (imm)",
                       qftasm_reg2addr(inst->dst.reg),
                       qftasm_int24_to_int16(inst->src.imm) + QFTASM_MEM_OFFSET);
    } else {
      error("Invalid value at STORE");
    }
    break;

  case PUTC:
#ifdef QFTASM_RAM_AS_STDOUT_BUFFER
    qftasm_emit_line("MNZ 1 %s A%d; PUTC", qftasm_src_str(inst), QFTASM_STDOUT);
    qftasm_emit_line("SUB A%d 1 %d;", QFTASM_STDOUT, QFTASM_STDOUT);
#else
    qftasm_emit_line("MNZ 1 %s %d; PUTC", qftasm_src_str(inst), QFTASM_STDOUT);
    qftasm_emit_line("MNZ 1 %d %d;", QFTASM_STDIO_CLOSED, QFTASM_STDOUT);
#endif
    break;

  case GETC:
#ifdef QFTASM_RAM_AS_STDIN_BUFFER
    qftasm_emit_line("MNZ 1 B%d %d; GETC", QFTASM_STDIN, qftasm_reg2addr(inst->dst.reg));
    qftasm_emit_line("SUB A%d 1 %d;", QFTASM_STDIN, QFTASM_STDIN);
#else
    qftasm_emit_line("MNZ 1 %d %d; GETC", QFTASM_STDIO_OPEN, QFTASM_STDIN);
    qftasm_emit_line("MNZ 0 0 0;"); // Required due to the delay between memory writing and instruction execution
    qftasm_emit_line("MNZ 1 A%d %d;", QFTASM_STDIN, qftasm_reg2addr(inst->dst.reg));
    qftasm_emit_line("MNZ 1 %d %d;", QFTASM_STDIO_CLOSED, QFTASM_STDIN);
#endif
    break;

  case EXIT:
    qftasm_emit_line("MNZ 1 65534 0; EXIT");
    qftasm_emit_line("MNZ 0 0 0;");
    break;

  case DUMP:
    break;

  case EQ:
    qftasm_emit_line("XOR %s A%d %d; EQ",
                     qftasm_src_str(inst),
                     qftasm_reg2addr(inst->dst.reg),
                     qftasm_reg2addr(inst->dst.reg));
    qftasm_emit_line("MNZ A%d 1 %d;", qftasm_reg2addr(inst->dst.reg), qftasm_reg2addr(inst->dst.reg));
    qftasm_emit_line("XOR 1 A%d %d;", qftasm_reg2addr(inst->dst.reg), qftasm_reg2addr(inst->dst.reg));
    break;
  case NE:
    qftasm_emit_line("XOR %s A%d %d; NE",
                     qftasm_src_str(inst), qftasm_reg2addr(inst->dst.reg), qftasm_reg2addr(inst->dst.reg));
    qftasm_emit_line("MNZ A%d 1 %d;", qftasm_reg2addr(inst->dst.reg), qftasm_reg2addr(inst->dst.reg));
    break;
  case LT:
    qftasm_emit_line("MNZ 1 0 %d; LT", QFTASM_TEMP);
    qftasm_emit_line("SUB A%d %s %d",
                     qftasm_reg2addr(inst->dst.reg), qftasm_src_str(inst), qftasm_reg2addr(inst->dst.reg));
    qftasm_emit_line("MLZ A%d 1 %d", qftasm_reg2addr(inst->dst.reg), QFTASM_TEMP);
    qftasm_emit_line("MNZ 1 A%d %d", QFTASM_TEMP, qftasm_reg2addr(inst->dst.reg));
    break;
  case GT:
    qftasm_emit_line("MNZ 1 0 %d; GT", QFTASM_TEMP);
    qftasm_emit_line("SUB %s %s %d",
                     qftasm_src_str(inst), qftasm_dst_str(inst), qftasm_reg2addr(inst->dst.reg));
    qftasm_emit_line("MLZ A%d 1 %d", qftasm_reg2addr(inst->dst.reg), QFTASM_TEMP);
    qftasm_emit_line("MNZ 1 A%d %d", QFTASM_TEMP, qftasm_reg2addr(inst->dst.reg));
    break;
  case LE:
    qftasm_emit_line("MNZ 1 0 %d; LE", QFTASM_TEMP);
    qftasm_emit_line("SUB %s %s %d",
                     qftasm_src_str(inst), qftasm_dst_str(inst), qftasm_reg2addr(inst->dst.reg));
    qftasm_emit_line("MLZ A%d 1 %d", qftasm_reg2addr(inst->dst.reg), QFTASM_TEMP);
    qftasm_emit_line("MNZ 1 A%d %d", QFTASM_TEMP, qftasm_reg2addr(inst->dst.reg));
    qftasm_emit_line("XOR 1 A%d %d", qftasm_reg2addr(inst->dst.reg), qftasm_reg2addr(inst->dst.reg));
    break;
  case GE:
    qftasm_emit_line("MNZ 1 0 %d; GE", QFTASM_TEMP);
    qftasm_emit_line("SUB %s %s %d",
                     qftasm_dst_str(inst), qftasm_src_str(inst), qftasm_reg2addr(inst->dst.reg));
    qftasm_emit_line("MLZ A%d 1 %d", qftasm_reg2addr(inst->dst.reg), QFTASM_TEMP);
    qftasm_emit_line("MNZ 1 A%d %d", QFTASM_TEMP, qftasm_reg2addr(inst->dst.reg));
    qftasm_emit_line("XOR 1 A%d %d", qftasm_reg2addr(inst->dst.reg), qftasm_reg2addr(inst->dst.reg));
    break;

  case JEQ:
    qftasm_emit_line("XOR %s A%d %d; JEQ",
                     qftasm_src_str(inst), qftasm_reg2addr(inst->dst.reg), QFTASM_TEMP);
    qftasm_emit_line("MNZ A%d 1 %d;", QFTASM_TEMP, QFTASM_TEMP);
    qftasm_emit_line("XOR 1 A%d %d;", QFTASM_TEMP, QFTASM_TEMP);
    qftasm_emit_conditional_jmp_inst(inst);
    break;
  case JNE:
    qftasm_emit_line("XOR %s A%d %d; JNE",
                     qftasm_src_str(inst), qftasm_reg2addr(inst->dst.reg), QFTASM_TEMP);
    qftasm_emit_conditional_jmp_inst(inst);
    break;
  case JLT:
    qftasm_emit_line("MNZ 1 0 %d; JLT", QFTASM_TEMP_2);
    qftasm_emit_line("SUB %s %s %d",
                     qftasm_dst_str(inst), qftasm_src_str(inst), QFTASM_TEMP);
    qftasm_emit_line("MLZ A%d 1 %d", QFTASM_TEMP, QFTASM_TEMP_2);
    qftasm_emit_line("MNZ 1 A%d %d", QFTASM_TEMP_2, QFTASM_TEMP);
    qftasm_emit_conditional_jmp_inst(inst);
    break;
  case JGT:
    qftasm_emit_line("MNZ 1 0 %d; JGT", QFTASM_TEMP_2);
    qftasm_emit_line("SUB %s %s %d",
                     qftasm_src_str(inst), qftasm_dst_str(inst), QFTASM_TEMP);
    qftasm_emit_line("MLZ A%d 1 %d", QFTASM_TEMP, QFTASM_TEMP_2);
    qftasm_emit_line("MNZ 1 A%d %d", QFTASM_TEMP_2, QFTASM_TEMP);
    qftasm_emit_conditional_jmp_inst(inst);
    break;
  case JLE:
    qftasm_emit_line("MNZ 1 0 %d; JLE", QFTASM_TEMP_2);
    qftasm_emit_line("SUB %s %s %d",
                     qftasm_src_str(inst), qftasm_dst_str(inst), QFTASM_TEMP);
    qftasm_emit_line("MLZ A%d 1 %d", QFTASM_TEMP, QFTASM_TEMP_2);
    qftasm_emit_line("MNZ 1 A%d %d", QFTASM_TEMP_2, QFTASM_TEMP);
    qftasm_emit_line("XOR 1 A%d %d", QFTASM_TEMP, QFTASM_TEMP);
    qftasm_emit_conditional_jmp_inst(inst);
    break;
  case JGE:
    qftasm_emit_line("MNZ 1 0 %d; GE", QFTASM_TEMP_2);
    qftasm_emit_line("SUB %s %s %d",
                     qftasm_dst_str(inst), qftasm_src_str(inst), QFTASM_TEMP);
    qftasm_emit_line("MLZ A%d 1 %d", QFTASM_TEMP, QFTASM_TEMP_2);
    qftasm_emit_line("MNZ 1 A%d %d", QFTASM_TEMP_2, QFTASM_TEMP);
    qftasm_emit_line("XOR 1 A%d %d", QFTASM_TEMP, QFTASM_TEMP);
    qftasm_emit_conditional_jmp_inst(inst);
    break;
  case JMP:
    qftasm_emit_unconditional_jmp_inst(inst);
    break;

  default:
    error("oops");
  }
}

void target_qftasm(Module* module) {
  init_state_qftasm(module->data, module->text);

  emit_chunked_main_loop(module->text,
                         qftasm_emit_func_prologue,
                         qftasm_emit_func_epilogue,
                         qftasm_emit_pc_change,
                         qftasm_emit_inst);
}
