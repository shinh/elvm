#include <ir/ir.h>
#include <target/util.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static char const* cmake_prefix(void) {
  char const* prefix = getenv("ELVM_CMAKE_PREFIX");
  return prefix ? prefix : "elvm";
}




/* Initializes registers, memory and input
 */
static void cmake_init_data(char const* indent, Data const* data) {

  /* Initialize registers
   */
  for (int i = 0; i < 7; i++) {
    emit_line("%sset_property(GLOBAL PROPERTY \"%s_reg_%s\" \"0\")", indent, cmake_prefix(), reg_names[i]);
  }
  emit_line("");


  /* Initialize memory
   */
  for (int mp = 0; data; data = data->next, mp++) {
    emit_line("%sset_property(GLOBAL PROPERTY \"%s_mem_%d\" \"%d\")", indent, cmake_prefix(), mp, data->v);
  }
  emit_line("");


  /* Initialize GETC source
   */
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_input\" \"\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_input_idx\" \"0\")", indent, cmake_prefix());
  emit_line("");

  emit_line("%sif (EXISTS \"${ELVM_INPUT_FILE}\")", indent);
  emit_line("%s\tfile(READ \"${ELVM_INPUT_FILE}\" \"input\" \"\" HEX)", indent, cmake_prefix());
  emit_line("%s\tstring(TOUPPER \"${input}\" \"input\")", indent);
  emit_line("%s\tset_property(GLOBAL PROPERTY \"%s_input\" \"${input}\")", indent, cmake_prefix());
  emit_line("%sendif()", indent);
  emit_line("");
}



static void cmake_emit_func_prologue(int func_id) {
  emit_line("# cmake_emit_func_prologue(%d)", func_id);
  emit_line("function(\"%s_chunk_%d\")", cmake_prefix(), func_id);

  /* Function chuck only coveres a part of the total program counter span
   */
  const int min_pc_incl = (func_id + 0) * CHUNKED_FUNC_SIZE;
  const int max_pc_excl = (func_id + 1) * CHUNKED_FUNC_SIZE;

  emit_line("\twhile (TRUE)");
  emit_line("\t\tget_property(\"pc\" GLOBAL PROPERTY \"%s_reg_pc\")", cmake_prefix());
  emit_line("\t\tget_property(\"running\" GLOBAL PROPERTY \"%s_running\")", cmake_prefix());
  emit_line("");
  emit_line("\t\tif (\"${pc}\" LESS \"%d\")", min_pc_incl);
  emit_line("\t\t\tbreak()");
  emit_line("\t\tendif()");
  emit_line("");
  emit_line("\t\tif (\"${pc}\" GREATER_EQUAL \"%d\")", max_pc_excl);
  emit_line("\t\t\tbreak()");
  emit_line("\t\tendif()");
  emit_line("");
  emit_line("\t\tif (\"${running}\" STREQUAL \"FALSE\")");
  emit_line("\t\t\tbreak()");
  emit_line("\t\tendif()");
  emit_line("");

  /* Dummy
   */
  emit_line("\t\t# Dummy before first basic block");
  emit_line("\t\tif (FALSE)");
}



/* Program counter must be increased at end of basic block
 */
static void cmake_emit_end_of_basic_block(void) {
  emit_line("\t\t\tget_property(\"pc\" GLOBAL PROPERTY \"%s_reg_pc\")", cmake_prefix());
  emit_line("\t\t\tmath(EXPR \"pc\" \"${pc} + 1\")");
  emit_line("\t\t\tset_property(GLOBAL PROPERTY \"%s_reg_pc\" \"${pc}\")", cmake_prefix());
  emit_line("\t\t\tcontinue()");
  emit_line("\t\tendif()");
}



static void cmake_emit_func_epilogue(void) {
  emit_line("\t\t\t# cmake_emit_func_epilogue()");
  cmake_emit_end_of_basic_block();
  emit_line("\tendwhile()");
  emit_line("endfunction()");
  emit_line("");
}



static void cmake_emit_pc_change(int pc) {
  emit_line("\t\t\t# cmake_emit_pc_change(%d)", pc);
  cmake_emit_end_of_basic_block();
  emit_line("");
  emit_line("\t\tget_property(\"pc\" GLOBAL PROPERTY \"%s_reg_pc\")", cmake_prefix());
  emit_line("\t\tif (\"${pc}\" EQUAL \"%d\")", pc);
}



static void cmake_emit_debug(char const* indent, char const* message) {
  emit_line("%s# %s", indent, message);
  emit_line("%sif (DEFINED ENV{DEBUG})", indent);
  emit_line("%s\tmessage(STATUS \"%s\")", indent, message);
  emit_line("%sendif()", indent);
  emit_line("");
}



/* Loads `value' into a local variable named `variable'
 *
 * @param indent Indent to use
 * @param variable Local CMake variable name
 * @param value Registry or immediate value
 *
 * @see value_str
 */
static void cmake_emit_read_value(
      char const* indent,
      char const* variable,
      Value const* value
    ) {

  if (REG == value->type) {
    emit_line("%sget_property(\"%s\" GLOBAL PROPERTY \"%s_reg_%s\")", indent, variable, cmake_prefix(), reg_names[value->reg]);

  } else if (IMM == value->type) {
    emit_line("%sset(\"%s\" \"%d\")", indent, variable, value->imm);

  } else {
    error("Invalid value");
  }
}



/* Stores the content of the local variable named `variable' into `value'
 */
static void cmake_emit_write_value(
      char const* indent,
      char const* variable,
      Value const* value
    ) {

  if (REG == value->type) {
    emit_line("%sset_property(GLOBAL PROPERTY \"%s_reg_%s\" \"${%s}\")", indent, cmake_prefix(), reg_names[value->reg], variable);

  } else {
    error("Invalid value");
  }
}



static char const* cmake_op_str(Op op) {
  switch (op) {
  case OP_UNSET:
  case OP_ERR:
  case LAST_OP: {
    error(format("Invalid opcode %d", op));
  } break;

  case MOV: return "MOV";
  case ADD: return "ADD";
  case SUB: return "SUB";
  case LOAD: return "LOAD";
  case STORE: return "STORE";
  case PUTC: return "PUTC";
  case GETC: return "GETC";
  case EXIT: return "EXIT";
  case JEQ: return "JEQ";
  case JNE: return "JNE";
  case JLT: return "JLT";
  case JGT: return "JGT";
  case JLE: return "JLE";
  case JGE: return "JGE";
  case JMP: return "JMP";
  case EQ: return "EQ";
  case NE: return "NE";
  case LT: return "LT";
  case GT: return "GT";
  case LE: return "LE";
  case GE: return "GE";
  case DUMP: return "DUMP";
  }

  error(format("Unsupported opcode %d", op));
}



static char const* cmake_condition(Op op, char const* a, char const* b) {
  switch (op) {
  case JEQ: return format("\"%s\" EQUAL \"%s\"", a, b);
  case JNE: return format("NOT (\"%s\" EQUAL \"%s\")", a, b);
  case JLT: return format("\"%s\" LESS \"%s\"", a, b);
  case JGT: return format("\"%s\" GREATER \"%s\"", a, b);
  case JLE: return format("\"%s\" LESS_EQUAL \"%s\"", a, b);
  case JGE: return format("\"%s\" GREATER_EQUAL \"%s\"", a, b);
  case EQ: return format("\"%s\" EQUAL \"%s\"", a, b);
  case NE: return format("NOT (\"%s\" EQUAL \"%s\")", a, b);
  case LT: return format("\"%s\" LESS \"%s\"", a, b);
  case GT: return format("\"%s\" GREATER \"%s\"", a, b);
  case LE: return format("\"%s\" LESS_EQUAL \"%s\"", a, b);
  case GE: return format("\"%s\" GREATER_EQUAL \"%s\"", a, b);

  default: {/* error */}
  }

  error(format("Opcode %d not supported for comparison", op));
}



static void cmake_emit_jump(char const* indent, char const* target) {

  /* Since the program counter is automatically increased at the end of every
   * basic block, we have to subtract this expected addition from the target
   * address here
   */
  emit_line("%smath(EXPR \"jmp_target\" \"%s - 1\")", indent, target);
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_reg_pc\" \"${jmp_target}\")", indent, cmake_prefix());
}



static void cmake_emit_inst(Inst* inst) {
  emit_line("\t\t\t# cmake_emit_inst(...)");
  switch (inst->op) {


  case MOV: {
    cmake_emit_debug("\t\t\t", format("MOV %s, %s", value_str(&inst->dst), value_str(&inst->src)));

    cmake_emit_read_value("\t\t\t", "mov_tmp", &inst->src);
    cmake_emit_write_value("\t\t\t", "mov_tmp", &inst->dst);
  } break;


  case ADD: {
    cmake_emit_debug("\t\t\t", format("ADD %s, %s", value_str(&inst->dst), value_str(&inst->src)));

    cmake_emit_read_value("\t\t\t", "add_a", &inst->dst);
    cmake_emit_read_value("\t\t\t", "add_b", &inst->src);
    emit_line("\t\t\tmath(EXPR \"add_sum\" \"(${add_a} + ${add_b}) & %s\")", UINT_MAX_STR);
    cmake_emit_write_value("\t\t\t", "add_sum", &inst->dst);
  } break;


  case SUB: {
    cmake_emit_debug("\t\t\t", format("SUB %s, %s", value_str(&inst->dst), value_str(&inst->src)));

    cmake_emit_read_value("\t\t\t", "sub_minuend", &inst->dst);
    cmake_emit_read_value("\t\t\t", "sub_subtrahend", &inst->src);
    emit_line("\t\t\tmath(EXPR \"sub_difference\" \"(${sub_minuend} - ${sub_subtrahend}) & %s\")", UINT_MAX_STR);
    cmake_emit_write_value("\t\t\t", "sub_difference", &inst->dst);
  } break;


  // dst = mem[src]
  case LOAD: {
    cmake_emit_debug("\t\t\t", format("LOAD %s, %s", value_str(&inst->dst), value_str(&inst->src)));

    cmake_emit_read_value("\t\t\t", "load_address", &inst->src);
    emit_line("\t\t\tget_property(\"load_address_set\" GLOBAL PROPERTY \"%s_mem_${load_address}\" SET)", cmake_prefix());
    emit_line("\t\t\tset(\"load_value\" \"0\")");
    emit_line("");

    emit_line("\t\t\tif (\"${load_address_set}\")");
    emit_line("\t\t\t\tget_property(\"load_value\" GLOBAL PROPERTY \"%s_mem_${load_address}\")", cmake_prefix());
    emit_line("\t\t\tendif()");
    emit_line("");

    cmake_emit_write_value("\t\t\t", "load_value", &inst->dst);
  } break;


  // mem[src] = dst;
  case STORE: {
    cmake_emit_debug("\t\t\t", format("STORE %s, %s", value_str(&inst->src), value_str(&inst->dst)));

    cmake_emit_read_value("\t\t\t", "store_address", &inst->src);
    cmake_emit_read_value("\t\t\t", "store_value", &inst->dst);
    emit_line("\t\t\tset_property(GLOBAL PROPERTY \"%s_mem_${store_address}\" \"${store_value}\")", cmake_prefix());
  } break;


  case PUTC: {
    cmake_emit_debug("\t\t\t", format("PUTC %s", value_str(&inst->src)));

    cmake_emit_read_value("\t\t\t", "putc_value_number", &inst->src);

/* string Character with code 0 does not exist.
    emit_line("\t\t\tstring(ASCII \"${putc_value_number}\" \"putc_value_string\")");
    emit_line("\t\t\texecute_process(COMMAND ${CMAKE_COMMAND} -E echo_append \"${putc_value_string}\")");
*/

/* math EXPR called with incorrect arguments.
    emit_line("\t\t\tmath(EXPR \"putc_value_hex\" \"${putc_value_number}\" HEXADECIMAL)");
    emit_line("\t\t\tstring(SUBSTRING \"${putc_value_hex}\" 1 3 \"putc_value_hex\")");
    emit_line("\t\t\texecute_process(COMMAND echo -ne \"\\x${putc_value_hex}\")");
*/

    // Currently using an external helper
    emit_line("\t\t\tif (NOT EXISTS \"${ELVM_PUTC_HELPER}\")");
    emit_line("\t\t\t\tmessage(FATAL_ERROR \"`ELVM_PUTC_HELPER' (${ELVM_PUTC_HELPER}) does not exist\")");
    emit_line("\t\t\tendif()");
    emit_line("\t\t\texecute_process(COMMAND \"${ELVM_PUTC_HELPER}\" \"${putc_value_number}\")");
  } break;


  case GETC: {
    cmake_emit_debug("\t\t\t", format("GETC %s", value_str(&inst->dst)));

    emit_line("\t\t\tget_property(\"getc_input\" GLOBAL PROPERTY \"%s_input\")", cmake_prefix());
    emit_line("\t\t\tget_property(\"getc_input_idx\" GLOBAL PROPERTY \"%s_input_idx\")", cmake_prefix());
    emit_line("\t\t\tstring(LENGTH \"${getc_input}\" \"getc_input_length\")");
    emit_line("");

    /* Read next character from input
     */
    emit_line("\t\t\tif (\"${getc_input_idx}\" LESS \"${getc_input_length}\")");
    emit_line("\t\t\t\tstring(SUBSTRING \"${getc_input}\" \"${getc_input_idx}\" \"2\" \"getc_next_string\")");
    emit_line("\t\t\t\tmath(EXPR \"getc_input_idx\" \"${getc_input_idx} + 2\")");
    emit_line("\t\t\t\tset_property(GLOBAL PROPERTY \"%s_input_idx\" \"${getc_input_idx}\")", cmake_prefix());
    emit_line("");

    emit_line("\t\t\t\tif (FALSE)");
    for (size_t i = 0; i <= UINT8_MAX; ++i) {
      emit_line("\t\t\t\telseif (\"${getc_next_string}\" STREQUAL \"%02X\")", i);
      emit_line("\t\t\t\t\tset(\"getc_next_number\" \"%d\")", i);
      emit_line("");
    }
    emit_line("\t\t\t\telse()");
    emit_line("\t\t\t\t\tmessage(FATAL_ERROR \"Invalid binary input ${getc_next_string}\")");
    emit_line("\t\t\t\tendif()");

    cmake_emit_write_value("\t\t\t\t", "getc_next_number", &inst->dst);

    /* EOF
     */
    emit_line("\t\t\telse()");
    emit_line("\t\t\t\tset(\"getc_eof\" \"0\")");
    cmake_emit_write_value("\t\t\t\t", "getc_eof", &inst->dst);
    emit_line("\t\t\tendif()");
  } break;


  case EXIT: {
    cmake_emit_debug("\t\t\t", "EXIT");
    emit_line("\t\t\tset_property(GLOBAL PROPERTY \"%s_running\" \"FALSE\")", cmake_prefix());
  } break;


  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE: {
    cmake_emit_debug("\t\t\t", format("%s %s, %s, %s", cmake_op_str(inst->op), value_str(&inst->jmp), value_str(&inst->dst), value_str(&inst->src)));

    cmake_emit_read_value("\t\t\t", "jmp_target", &inst->jmp);
    cmake_emit_read_value("\t\t\t", "jmp_compare_a", &inst->dst);
    cmake_emit_read_value("\t\t\t", "jmp_compare_b", &inst->src);
    emit_line("");
    emit_line("\t\t\tif (%s)", cmake_condition(inst->op, "${jmp_compare_a}", "${jmp_compare_b}"));
    cmake_emit_jump("\t\t\t\t", "${jmp_target}");
    emit_line("\t\t\tendif()");
  } break;


  case JMP: {
    cmake_emit_debug("\t\t\t", format("JMP %s", value_str(&inst->jmp)));

    cmake_emit_read_value("\t\t\t", "jmp_target", &inst->jmp);
    cmake_emit_jump("\t\t\t", "${jmp_target}");
  } break;


  case EQ:
  case NE:
  case LT:
  case GT:
  case LE:
  case GE: {
    cmake_emit_debug("\t\t\t", format("%s %s, %s", cmake_op_str(inst->op), value_str(&inst->dst), value_str(&inst->src)));

    cmake_emit_read_value("\t\t\t", "compare_a", &inst->dst);
    cmake_emit_read_value("\t\t\t", "compare_b", &inst->src);
    emit_line("\t\t\tset(\"compare_result\" \"0\")");
    emit_line("");

    emit_line("\t\t\tif (%s)", cmake_condition(inst->op, "${compare_a}", "${compare_b}"));
    emit_line("\t\t\t\tset(\"compare_result\" \"1\")");
    emit_line("\t\t\tendif()");
    emit_line("");

    cmake_emit_write_value("\t\t\t", "compare_result", &inst->dst);
  } break;


  case DUMP: {
    cmake_emit_debug("\t\t\t", "DUMP");
    emit_line("\t\t\t# (no-op)");
  } break;


  default:
    emit_line("\t\t\tmessage(FATAL_ERROR \"Invalid opcode %d\")", inst->op);
    error(format("Invalid opcode %d", inst->op));
    break;
  }

  emit_line("");
}



void target_cmake(Module* module) {
  emit_line("cmake_minimum_required(VERSION %s)", "3.10");
  emit_line("");

  /* Emit definition of all instructions, chunked into groups of basic blocks
   */
  int const num_funcs = emit_chunked_main_loop(
    module->text,
    cmake_emit_func_prologue,
    cmake_emit_func_epilogue,
    cmake_emit_pc_change,
    cmake_emit_inst
  );

  /* Define main function, can be invoked multiple times since memory and
   * register initialization is done on each call
   */
  emit_line("function(\"%s_main\")", cmake_prefix());
  cmake_init_data("\t", module->data);

  emit_line("\twhile (TRUE)");

  /* Check if still running
   */
  emit_line("\t\tget_property(\"running\" GLOBAL PROPERTY \"%s_running\")", cmake_prefix());
  emit_line("");
  emit_line("\t\tif (\"${running}\" STREQUAL \"FALSE\")");
  emit_line("\t\t\tbreak()");
  emit_line("\t\tendif()");
  emit_line("");

  /* Choose correct chunk to execute
   */
  emit_line("\t\tget_property(\"pc\" GLOBAL PROPERTY \"%s_reg_pc\")", cmake_prefix());
  emit_line("\t\tmath(EXPR \"chunk\" \"${pc} / %d\")", CHUNKED_FUNC_SIZE);
  emit_line("");

  for (int func = 0; func < num_funcs; ++func) {
    emit_line("\t\tif (\"%d\" EQUAL \"${chunk}\")", func);
    emit_line("\t\t\t%s_chunk_%d()", cmake_prefix(), func);
    emit_line("\t\t\tcontinue()");
    emit_line("\t\tendif()");
  }
  emit_line("\tendwhile()");
  emit_line("endfunction()");
  emit_line("");

  /* Auto invoke main function
   */
  emit_line("%s_main()", cmake_prefix());
}

