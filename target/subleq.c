#include <ir/ir.h>
#include <target/util.h>
#include <stdint.h>

#define SUBLEQ_ONE (11)
#define SUBLEQ_NEG_ONE (10)
#define SUBLEQ_MEM_START (12)
#define SUBLEQ_JTABLE_START (13)
#define SUBLEQ_REG(regnum) (regnum + 3)

typedef struct {
  int32_t word_on;
  int32_t mem_start;
  int32_t jump_table_start;
} SubleqGen;

static SubleqGen subleq;

static void subleq_emit_instr(int32_t a, int32_t b, int32_t c){
  emit_line("%d %d %d", a, b, c);
  subleq.word_on += 3;
}

static void subleq_emit_sub(int32_t b, int32_t a){
  subleq.word_on += 3;
  emit_line("%d %d %d", a, b, subleq.word_on);
}

static void init_state_subleq(Data* data) {

  subleq.word_on = 0;

  // Data length always includes 7 registers and 4 constants
  size_t data_length = 11;
  for (Data* i = data; i->next != NULL; i = i->next){
    data_length++;
  }

  subleq_emit_instr(0, 0, data_length + 3);
  // Emit registers
  emit_line("0 0 0 0 0 0 0");
  // Emit constants
  emit_line("-1 1 %d %d", subleq.mem_start, subleq.jump_table_start);
  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      emit_line("%d", data->v);
    }
  }

  subleq.word_on += data_length + 1;

}

static void subleq_emit_inst(Inst* inst) {
  switch (inst->op) {
  case MOV:
    //emit_line("%s = %s", reg_names[inst->dst.reg], src_str(inst));
    break;

  case ADD:
    // emit_line("%s = (%s + %s) & " UINT_MAX_STR,
    //           reg_names[inst->dst.reg],
    //           reg_names[inst->dst.reg], src_str(inst));
    break;

  case SUB:
    subleq_emit_sub(SUBLEQ_REG(inst->dst.reg), 2);
    // emit_line("%s = (%s - %s) & " UINT_MAX_STR,
    //           reg_names[inst->dst.reg],
    //           reg_names[inst->dst.reg], src_str(inst));
    break;

  case LOAD:
    // emit_line("%s = mem[%s]", reg_names[inst->dst.reg], src_str(inst));
    break;

  case STORE:
    // emit_line("mem[%s] = %s", src_str(inst), reg_names[inst->dst.reg]);
    break;

  case PUTC:
    //emit_line("sys.stdout.write(chr(%s))", src_str(inst));
    break;

  case GETC:
    // emit_line("_ = sys.stdin.read(1); %s = ord(_) if _ else 0",
    //           reg_names[inst->dst.reg]);
    break;

  case EXIT:
    subleq_emit_instr(0, 0, -1);
    break;

  case DUMP:
    break;

  case EQ:
  case NE:
  case LT:
  case GT:
  case LE:
  case GE:
    // emit_line("%s = int(%s)",
    //           reg_names[inst->dst.reg], cmp_str(inst, "True"));
    break;

  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
  case JMP:
    // emit_line("if %s: pc = %s - 1",
    //           cmp_str(inst, "True"), value_str(&inst->jmp));
    break;

  default:
    error("oops");
  }
}

void target_subleq(Module* module) {
  // init_state_py(module->data);

  //int num_funcs = emit_chunked_main_loop(module->text,
  //                                        py_emit_func_prologue,
  //                                        py_emit_func_epilogue,
  //                                        py_emit_pc_change,
  //                                        py_emit_inst);

  // emit_line("");
  // emit_line("while True:");
  // inc_indent();
  // emit_line("if False: pass");
  // for (int i = 0; i < num_funcs; i++) {
  //   emit_line("elif pc < %d: func%d()", (i + 1) * CHUNKED_FUNC_SIZE, i);
  // }
  // dec_indent();

  emit_reset();
  init_state_subleq(module->data);

  int prev_pc = -1;
  for (Inst* inst = module->text; inst; inst = inst->next) {
    if (prev_pc != inst->pc) {
      if (prev_pc == -1) {
        // TODO: Emit start of Insts
      } else {
        // TODO: Emit end of old PC block and start of new one
      }
    }
    prev_pc = inst->pc;
    subleq_emit_inst(inst);
  }

  emit_reset();
  emit_start();

}
