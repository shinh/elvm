#include <ir/ir.h>
#include <target/util.h>

static const char* REG_NAMES_VIM[] = {
  "s:a", "s:b", "s:c", "s:d", "s:bp", "s:sp", "s:pc"
};

static void init_state_vim(Data* data) {
  reg_names = REG_NAMES_VIM;
  for (int i = 0; i < 7; i++) {
    emit_line("let %s = 0", reg_names[i]);
  }
  emit_line("let s:mem = repeat([0], 16777216)");
  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      emit_line("let s:mem[%d] = %d", mp, data->v);
    }
  }
  emit_line("let s:input = map(split(join(getline(1, '$'), \"\\n\"), '\\zs'), 'char2nr(v:val)')");
  emit_line("let s:ic = 0");
  emit_line("let s:output = []");
  emit_line("normal! dG");
}

static void vim_emit_func_prologue(int func_id) {
  emit_line("");
  emit_line("function! Func%d()", func_id);
  inc_indent();
  emit_line("while %d <= s:pc && s:pc < %d",
            func_id * CHUNKED_FUNC_SIZE, (func_id + 1) * CHUNKED_FUNC_SIZE);
  inc_indent();
  emit_line("if 0");
  inc_indent();
}

static void vim_emit_func_epilogue(void) {
  dec_indent();
  emit_line("endif");
  emit_line("let s:pc += 1");
  dec_indent();
  emit_line("endwhile");
  dec_indent();
  emit_line("endfunction");
}

static void vim_emit_pc_change(int pc) {
  emit_line("");
  dec_indent();
  emit_line("elseif s:pc == %d", pc);
  inc_indent();
}

static void vim_emit_inst(Inst* inst) {
  switch (inst->op) {
  case MOV:
    emit_line("let %s = %s", reg_names[inst->dst.reg], src_str(inst));
    break;

  case ADD:
    emit_line("let %s = and((%s + %s), " UINT_MAX_STR ")",
              reg_names[inst->dst.reg],
              reg_names[inst->dst.reg], src_str(inst));
    break;

  case SUB:
    emit_line("let %s = and((%s - %s), " UINT_MAX_STR ")",
              reg_names[inst->dst.reg],
              reg_names[inst->dst.reg], src_str(inst));
    break;

  case LOAD:
    emit_line("let %s = s:mem[%s]", reg_names[inst->dst.reg], src_str(inst));
    break;

  case STORE:
    emit_line("let s:mem[%s] = %s", src_str(inst), reg_names[inst->dst.reg]);
    break;

  case PUTC:
    emit_line("let s:output += [%s]", src_str(inst));
    break;

  case GETC:
    emit_line("let %s = len(s:input) <= s:ic ? 0 : s:input[s:ic]", reg_names[inst->dst.reg]);
    emit_line("let s:ic += 1");
    break;

  case EXIT:
    emit_line("if len(s:output) == 0 | return 1 | endif");
    emit_line("let s:lines = ['']");
    emit_line("for s:ch in s:output");
    inc_indent();
    emit_line("if s:ch == 10");
    inc_indent();
    emit_line("let s:lines += ['']");
    dec_indent();
    emit_line("else");
    inc_indent();
    emit_line("let s:lines[len(s:lines)-1] .= nr2char(s:ch == 0 ? 10 : s:ch)");
    dec_indent();
    emit_line("endif");
    emit_line("unlet s:ch");
    dec_indent();
    emit_line("endfor");
    emit_line("call setline(1, s:lines)");
    emit_line("return 1");
    break;

  case DUMP:
    break;

  case EQ:
  case NE:
  case LT:
  case GT:
  case LE:
  case GE:
    emit_line("let %s = %s ? 1 : 0",
              reg_names[inst->dst.reg], cmp_str(inst, "1"));
    break;

  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
  case JMP:
    emit_line("if %s", cmp_str(inst, "1"));
    inc_indent();
    emit_line("let s:pc = %s - 1", value_str(&inst->jmp));
    dec_indent();
    emit_line("endif");
    break;

  default:
    error("oops");
  }
}

void target_vim(Module* module) {
  init_state_vim(module->data);
  emit_line("");

  int num_funcs = emit_chunked_main_loop(module->text,
                                         vim_emit_func_prologue,
                                         vim_emit_func_epilogue,
                                         vim_emit_pc_change,
                                         vim_emit_inst);

  emit_line("");
  emit_line("while 1");
  inc_indent();
  emit_line("if 0");
  for (int i = 0; i < num_funcs; i++) {
    emit_line("elseif s:pc < %d", (i + 1) * CHUNKED_FUNC_SIZE);
    inc_indent();
    emit_line("if Func%d() | break | endif", i);
    dec_indent();
  }
  emit_line("endif");
  dec_indent();
  emit_line("endwhile");
}
