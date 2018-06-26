#include <ir/ir.h>
#include <target/util.h>

// WebAssembly memory is specified in 64KiB pages
#define WASM_MEM_SIZE_IN_PAGES 1024

static void wasm_init_state(void) {
  emit_line("(module");
  inc_indent();
  emit_line("(import \"env\" \"getchar\" (func $getchar (result i32)))");
  emit_line("(import \"env\" \"putchar\" (func $putchar (param i32)))");
  emit_line("(import \"env\" \"exit\" (func $exit))");
  for (int i = 0; i < 7; i++) {
    emit_line("(global $%s (mut i32) (i32.const 0))", reg_names[i]);
  }
  emit_line("(memory %d)", WASM_MEM_SIZE_IN_PAGES);
}

static void wasm_emit_func_prologue(int func_id) {
  emit_line("");
  emit_line("(func $func%d", func_id);
  inc_indent();
  emit_line("(loop $while0");
  inc_indent();
  emit_line("(if (i32.and (i32.le_s (i32.const %d) (get_global $pc))",
            func_id * CHUNKED_FUNC_SIZE);
  emit_line("             (i32.lt_s (get_global $pc) (i32.const %d)))",
            (func_id + 1) * CHUNKED_FUNC_SIZE);
  inc_indent();
  emit_line("(then");
  inc_indent();
  emit_line("(block $while0body");
  inc_indent();
  // dummy first case
  emit_line("(if (i32.eq (get_global $pc) (i32.const -1))");
  inc_indent();
  emit_line("(then");
  inc_indent();
}

static void wasm_emit_func_epilogue(void) {
  emit_line("(br $while0body)");
  dec_indent();
  emit_line(")"); // then
  dec_indent();
  emit_line(")"); // if
  emit_line("");
  dec_indent();
  emit_line(")"); // block $while0body
  emit_line("(set_global $pc (i32.add (get_global $pc) (i32.const 1)))");
  emit_line("(br $while0)");
  dec_indent();
  emit_line(")"); // then
  dec_indent();
  emit_line(")"); // if
  dec_indent();
  emit_line(")"); // loop $while0
  dec_indent();
  emit_line(")"); // func
}

static void wasm_emit_pc_change(int pc) {
  emit_line("(br $while0body)");
  dec_indent();
  emit_line(")"); // then
  dec_indent();
  emit_line(")"); // if
  emit_line("");
  emit_line("(if (i32.eq (get_global $pc) (i32.const %d))", pc);
  inc_indent();
  emit_line("(then");
  inc_indent();
}

static const char* wasm_get_value(Value *v) {
  if (v->type == REG) {
    return format("(get_global $%s)", reg_names[v->reg]);
  } else if (v->type == IMM) {
    return format("(i32.const %d)", v->imm);
  } else {
    error("invalid src type");
  }
}

static const char* wasm_cmp_expr(Inst* inst) {
  int op = normalize_cond(inst->op, 0);
  const char* op_str;
  switch (op) {
    case JEQ:
      op_str = "i32.eq"; break;
    case JNE:
      op_str = "i32.ne"; break;
    case JLT:
      op_str = "i32.lt_s"; break;
    case JGT:
      op_str = "i32.gt_s"; break;
    case JLE:
      op_str = "i32.le_s"; break;
    case JGE:
      op_str = "i32.ge_s"; break;
    default:
      error("oops");
  }
  return format("(%s (get_global $%s) %s)",
                op_str, reg_names[inst->dst.reg], wasm_get_value(&inst->src));
}

static void wasm_emit_inst(Inst* inst) {
  switch (inst->op) {
  case MOV:
    emit_line("(set_global $%s %s)", reg_names[inst->dst.reg], wasm_get_value(&inst->src));
    break;

  case ADD:
    emit_line("(set_global $%s (i32.and (i32.add (get_global $%s) %s) (i32.const " UINT_MAX_STR ")))",
              reg_names[inst->dst.reg], reg_names[inst->dst.reg], wasm_get_value(&inst->src));
    break;

  case SUB:
    emit_line("(set_global $%s (i32.and (i32.sub (get_global $%s) %s) (i32.const " UINT_MAX_STR ")))",
              reg_names[inst->dst.reg], reg_names[inst->dst.reg], wasm_get_value(&inst->src));
    break;

  case LOAD:
    emit_line("(set_global $%s (i32.load (i32.shl %s (i32.const 2))))",
              reg_names[inst->dst.reg], wasm_get_value(&inst->src));
    break;

  case STORE:
    emit_line("(i32.store (i32.shl %s (i32.const 2)) (get_global $%s))",
              wasm_get_value(&inst->src), reg_names[inst->dst.reg]);
    break;

  case PUTC:
    emit_line("(call $putchar %s)", wasm_get_value(&inst->src));
    break;

  case GETC:
    emit_line("(set_global $%s (call $getchar))", reg_names[inst->dst.reg]);
    break;

  case EXIT:
    emit_line("(call $exit)");
    break;

  case DUMP:
    break;

  case EQ:
  case NE:
  case LT:
  case GT:
  case LE:
  case GE:
    emit_line("(set_global $%s %s)", reg_names[inst->dst.reg], wasm_cmp_expr(inst));
    break;

  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
    emit_line("(if %s (then (set_global $pc %s) (br $while0)))",
              wasm_cmp_expr(inst), wasm_get_value(&inst->jmp));
    break;

  case JMP:
    emit_line("(set_global $pc %s)", wasm_get_value(&inst->jmp));
    emit_line("(br $while0)");
    break;

  default:
    error("oops");
  }
}

void target_wasm(Module* module) {
  wasm_init_state();

  int num_funcs = emit_chunked_main_loop(module->text,
                                         wasm_emit_func_prologue,
                                         wasm_emit_func_epilogue,
                                         wasm_emit_pc_change,
                                         wasm_emit_inst);

  emit_line("");
  emit_line("(table anyfunc");
  inc_indent();
  emit_line("(elem");
  inc_indent();
  for (int i = 0; i < num_funcs; i++) {
    emit_line("$func%d", i);
  }
  dec_indent();
  emit_line(")"); // elem
  dec_indent();
  emit_line(")"); // table

  emit_line("");
  emit_line("(func (export \"wasmmain\")");
  inc_indent();
  emit_line("(local $chunk i32)");

  Data* data = module->data;
  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      emit_line("(i32.store (i32.shl (i32.const %d) (i32.const 2)) (i32.const %d))", mp, data->v);
    }
  }

  emit_line("");
  emit_line("(loop $mainloop");
  inc_indent();
  emit_line("(call_indirect (i32.div_u (get_global $pc) (i32.const %d)))", CHUNKED_FUNC_SIZE);
  emit_line("(br $mainloop)");
  dec_indent();
  emit_line(")"); // loop $mainloop

  dec_indent();
  emit_line(")"); // func wasmmain
  dec_indent();
  emit_line(")"); // module
}
