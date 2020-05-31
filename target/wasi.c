#include <ir/ir.h>
#include <target/util.h>

// WebAssembly memory is specified in 64KiB pages
// 1024 + 1
#define WASI_MEM_SIZE_IN_PAGES 1025

static const char* WASI_REG_NAMES[] = {
  "$a", "$b", "$c", "$d", "$bp", "$sp", "$pc"
};

static void wasi_init_memory(Data* data) {
    emit_line("(func $init_memory");
    inc_indent();
    emit_line("(i32.store (i32.add (i32.mul (i32.const 4) (i32.const %s)) (i32.const 4)) (i32.const 0)) ;; buffer",
              UINT_MAX_STR);
    emit_line("(i32.store (i32.add (i32.mul (i32.const 4) (i32.const %s)) (i32.const 8)) (i32.add (i32.mul (i32.const 4) (i32.const %s)) (i32.const 4))) ;; buf pointer",
              UINT_MAX_STR, UINT_MAX_STR);
    emit_line("(i32.store (i32.add (i32.mul (i32.const 4) (i32.const %s)) (i32.const 12)) (i32.const 1)) ;; buf length",
              UINT_MAX_STR);
    // mem[n]: i32.store (i32.mul (4) (n)) (x)
    for (int mp = 0; data; data = data->next, mp++) {
        if (data->v) {
            emit_line("(i32.store (i32.mul (i32.const 4) (i32.const %d)) (i32.const %d))", mp, data->v);
        }
    }
    dec_indent();
    emit_line(") ;; func init_memory");
}

static void wasi_emit_func_prologue(int func_id) {
    emit_line("");
    emit_line("(func $f%d", func_id);
    inc_indent();
    emit_line("(loop $loop0");
    inc_indent();
    // "for %d <= pc && pc < %d {", func_id * CHUNKED_FUNC_SIZE, (func_id + 1) * CHUNKED_FUNC_SIZE
    emit_line("(br_if 1 (i32.or (i32.gt_u (i32.const %d) (get_global $pc)) (i32.ge_u (get_global $pc) (i32.const %d))))",
              func_id * CHUNKED_FUNC_SIZE, (func_id + 1) * CHUNKED_FUNC_SIZE);
    emit_line("(if");
    inc_indent();
    emit_line("(i32.eqz (i32.const 1))");
    emit_line("(then");
    inc_indent();
    emit_line("(nop)");
}

static void wasi_emit_func_epilogue(void) {
    dec_indent();
    emit_line(") ;; then");
    dec_indent();
    emit_line(") ;; if");
    emit_line("(set_global $pc (i32.add (get_global $pc) (i32.const 1)))");
    emit_line("(br $loop0)");
    dec_indent();
    emit_line(") ;; loop $loop0");
    dec_indent();
    emit_line(") ;; func $f d");
}

static void wasi_emit_pc_change(int pc) {
    dec_indent();
    emit_line(") ;; then");
    dec_indent();
    emit_line(") ;; if");
    emit_line("(if");
    inc_indent();
    emit_line("(i32.eq (get_global $pc) (i32.const %d))", pc);
    emit_line("(then");
    inc_indent();
}

// wasm_get_value
static const char* wasi_get_value(Value *v) {
  if (v->type == REG) {
    return format("(get_global %s)", reg_names[v->reg]);
  } else if (v->type == IMM) {
    return format("(i32.const %d)", v->imm);
  } else {
    error("invalid src type");
  }
}

const char* wasi_cmp_str(Inst* inst) {
  const char* op_str;
  switch (inst->op) {
    case JEQ:
    case EQ:
      op_str = "i32.eq"; break;
    case JNE:
    case NE:
      op_str = "i32.ne"; break;
    case JLT:
    case LT:
      op_str = "i32.lt_u"; break;
    case JGT:
    case GT:
      op_str = "i32.gt_u"; break;
    case JLE:
    case LE:
      op_str = "i32.le_u"; break;
    case JGE:
    case GE:
      op_str = "i32.ge_u"; break;
    case JMP:
      return "(i32.eqz (i32.const 0))";
    default:
      error("oops");
  }
  return format("(%s (get_global %s) %s)", op_str, reg_names[inst->dst.reg], wasi_get_value(&inst->src));
}

static void wasi_emit_inst(Inst* inst) {
    switch (inst->op) {
    case MOV:
        emit_line("(set_global %s %s)", reg_names[inst->dst.reg], wasi_get_value(&inst->src));
        break;

    case ADD:
        emit_line("(set_global %s (i32.and (i32.add (get_global %s) %s) (i32.const " UINT_MAX_STR ")))",
                  reg_names[inst->dst.reg], reg_names[inst->dst.reg], wasi_get_value(&inst->src));
        break;

    case SUB:
        emit_line("(set_global %s (i32.and (i32.sub (get_global %s) %s) (i32.const " UINT_MAX_STR ")))",
                  reg_names[inst->dst.reg], reg_names[inst->dst.reg], wasi_get_value(&inst->src));
        break;

    case LOAD:
        emit_line("(set_global %s (i32.load (i32.mul (i32.const 4) %s)))",
                  reg_names[inst->dst.reg], wasi_get_value(&inst->src));
        break;

    case STORE:
        emit_line("(i32.store (i32.mul (i32.const 4) %s) (get_global %s))",
                  wasi_get_value(&inst->src), reg_names[inst->dst.reg]);
        break;

    case PUTC:
        emit_line("(i32.store (i32.add (i32.mul (i32.const 4) (i32.const %s)) (i32.const 4)) %s)",
                  UINT_MAX_STR, wasi_get_value(&inst->src));
        emit_line("(drop (call $__wasi_fd_write (i32.const 1) (i32.add (i32.mul (i32.const 4) (i32.const %s)) (i32.const 8)) (i32.const 1) (i32.add (i32.mul (i32.const 4) (i32.const %s)) (i32.const 16))))",
                  UINT_MAX_STR, UINT_MAX_STR);
        emit_line("(i32.store (i32.add (i32.mul (i32.const 4) (i32.const %s)) (i32.const 4)) (i32.const 0))",
                  UINT_MAX_STR);
        break;

    case GETC:
        emit_line("(i32.store (i32.add (i32.mul (i32.const 4) (i32.const %s)) (i32.const 4)) (i32.const 0))",
                  UINT_MAX_STR);
        emit_line("(drop (call $__wasi_fd_read (i32.const 0) (i32.add (i32.mul (i32.const 4) (i32.const %s)) (i32.const 8)) (i32.const 1) (i32.add (i32.mul (i32.const 4) (i32.const %s)) (i32.const 16))))",
                  UINT_MAX_STR, UINT_MAX_STR);
        emit_line("(set_global %s (i32.load (i32.add (i32.mul (i32.const 4) (i32.const %s)) (i32.const 4))))",
                  reg_names[inst->dst.reg], UINT_MAX_STR);
        break;

    case EXIT:
        emit_line("(call $__wasi_proc_exit (i32.const 0))");
        break;

    case DUMP:
        emit_line("(nop) ;; op DUMP");
        break;

    case EQ:
    case NE:
    case LT:
    case GT:
    case LE:
    case GE:
        emit_line("(if");
        inc_indent();
        emit_line("%s", wasi_cmp_str(inst));
        emit_line("(then");
        inc_indent();
        emit_line("(set_global %s (i32.const 1))", reg_names[inst->dst.reg]);
        dec_indent();
        emit_line(") ;; then");
        emit_line("(else");
        inc_indent();
        emit_line("(set_global %s (i32.const 0))", reg_names[inst->dst.reg]);
        dec_indent();
        emit_line(") ;; else");
        dec_indent();
        emit_line(") ;; if");
        break;

    case JEQ:
    case JNE:
    case JLT:
    case JGT:
    case JLE:
    case JGE:
    case JMP:
        emit_line("(if");
        inc_indent();
        emit_line("%s", wasi_cmp_str(inst));
        emit_line("(then");
        inc_indent();
        emit_line("(set_global $pc %s)", wasi_get_value(&inst->jmp));
        emit_line("(br $loop0)");
        dec_indent();
        emit_line(") ;; then");
        dec_indent();
        emit_line(") ;; if");
        break;

    default:
        error("oops");
    }
}

void target_wasi(Module* module) {
    emit_line("(module");
    inc_indent();
    emit_line("(import \"wasi_unstable\" \"fd_write\" (func $__wasi_fd_write (param i32 i32 i32 i32) (result i32)))");
    emit_line("(import \"wasi_unstable\" \"fd_read\" (func $__wasi_fd_read (param i32 i32 i32 i32) (result i32)))");
    emit_line("(import \"wasi_unstable\" \"proc_exit\" (func $__wasi_proc_exit (param i32)))");
    emit_line("(memory (export \"memory\") %d)", WASI_MEM_SIZE_IN_PAGES);

    reg_names = WASI_REG_NAMES;
    for (int i = 0; i < 7; i++) {
        emit_line("(global %s (mut i32) (i32.const 0))", reg_names[i]);
    }

    wasi_init_memory(module->data);

    int num_funcs = emit_chunked_main_loop(module->text, wasi_emit_func_prologue, wasi_emit_func_epilogue, wasi_emit_pc_change, wasi_emit_inst);

    emit_line("");
    emit_line("(table anyfunc");
    inc_indent();
    emit_line("(elem");
    inc_indent();
    for (int i = 0; i < num_funcs; i++) {
        emit_line("$f%d", i);
    }
    dec_indent();
    emit_line(") ;; elem");
    dec_indent();
    emit_line(") ;; table");
    emit_line("");
    emit_line("(func $main");
    inc_indent();
    emit_line("(call $init_memory)");
    emit_line("(loop $loop_main");
    inc_indent();
    emit_line("(call_indirect (i32.div_u (get_global $pc) (i32.const %d)))", CHUNKED_FUNC_SIZE);
    emit_line("(br $loop_main)");
    dec_indent();
    emit_line(") ;; loop $loop_main");
    dec_indent();
    emit_line(") ;; func main");
    emit_line("(export \"_start\" (func $main))");
    dec_indent();
    emit_line(") ;; module");
}
