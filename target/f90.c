#include <ir/ir.h>
#include <target/util.h>

static const char *fortran_cmp_str(Inst *inst) {
  int op = normalize_cond(inst->op, 0);
  const char* op_str;
  switch (op) {
    case JEQ:
      op_str = "=="; break;
    case JNE:
      op_str = "/="; break;
    case JLT:
      op_str = "<"; break;
    case JGT:
      op_str = ">"; break;
    case JLE:
      op_str = "<="; break;
    case JGE:
      op_str = ">="; break;
    case JMP:
      return ".true.";
    default:
       error("oops");
  }
  return format("%s %s %s", reg_names[inst->dst.reg], op_str, src_str(inst));
}

static void init_state_f90(Data *data) {
  emit_line("module data");
  inc_indent();
  emit_line("implicit none");
  emit_line("integer, dimension(0:ishft(1, 24)-1) :: mem = 0");
  
  for (int i = 0; i < 7; i++) {
    emit_line("integer :: %s = 0", reg_names[i]);
  }

  emit_line("contains");
  inc_indent();

  emit_line("subroutine init");

  inc_indent();

  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      emit_line("mem(%d) = %d", mp, data->v);
    }
  }

  dec_indent();

  emit_line("end subroutine init");

  dec_indent();
  dec_indent();
  emit_line("end module data");
}

static void f90_emit_func_prologue(int func_id) {
  emit_line("");
  emit_line("subroutine func%d", func_id);

  inc_indent();
  emit_line("use data");
  emit_line("implicit none");
  emit_line("character :: chr");

  emit_line("");
  emit_line("do while (%d <= pc .and. pc < %d)",
            func_id * CHUNKED_FUNC_SIZE, (func_id + 1) * CHUNKED_FUNC_SIZE);
  inc_indent();
  emit_line("select case (pc)");
  inc_indent();
  emit_line("case (-1)");
  inc_indent();
}

static void f90_emit_func_epilogue(void) {
  dec_indent();
  dec_indent();
  emit_line("end select");
  emit_line("pc = pc + 1");
  dec_indent();
  emit_line("end do");
  dec_indent();
  emit_line("end subroutine");
}

static void f90_emit_pc_change(int pc) {
  dec_indent();
  emit_line("case (%d)", pc);
  inc_indent();
}

static void f90_emit_inst(Inst* inst) {
  static int inputLabel = 1;

  switch (inst->op) {
    case MOV:
      emit_line("%s = %s", reg_names[inst->dst.reg], src_str(inst));
      break;
    
    case ADD:
      emit_line("%s = iand(%s + %s, " UINT_MAX_STR ")",
                reg_names[inst->dst.reg],
                reg_names[inst->dst.reg], src_str(inst));
      break;
    
    case SUB:
      emit_line("%s = iand(%s - %s, " UINT_MAX_STR ")",
                reg_names[inst->dst.reg],
                reg_names[inst->dst.reg], src_str(inst));
      break;
    
    case LOAD:
      emit_line("%s = mem(%s)", reg_names[inst->dst.reg], src_str(inst));
      break;

    case STORE:
      emit_line("mem(%s) = %s", src_str(inst), reg_names[inst->dst.reg]);
      break;

    case PUTC:
      emit_line("write(*, '(A)', advance='no') char(%s)",
                src_str(inst),
                inputLabel,
                inputLabel + 1);
      emit_line("");
      inputLabel += 2;
      break;

    case GETC:
      emit_line("read(*, '(A1)', eor=%d, end=%d, advance='no') chr",
                inputLabel,
                inputLabel + 1);
      emit_line("%s = ichar(chr)", reg_names[inst->dst.reg]);
      emit_line("goto %d", inputLabel + 2);
      emit_line("%d %s = 10", inputLabel, reg_names[inst->dst.reg]);
      emit_line("goto %d", inputLabel + 2);
      emit_line("%d %s = 0", inputLabel + 1, reg_names[inst->dst.reg]);
      emit_line("%d a = a", inputLabel + 2);
      inputLabel += 3;
      break;

    case EXIT:
      emit_line("stop");
      break;

    case DUMP:
      break;

    case EQ:
    case NE:
    case LT:
    case GT:
    case LE:
    case GE:
      emit_line("if (%s) then", fortran_cmp_str(inst));
      inc_indent();
      emit_line("%s = 1", reg_names[inst->dst.reg]);
      dec_indent();
      emit_line("else");
      inc_indent();
      emit_line("%s = 0", reg_names[inst->dst.reg]);
      dec_indent();
      emit_line("end if");
      break;

    case JEQ:
    case JNE:
    case JLT:
    case JGT:
    case JLE:
    case JGE:
    case JMP:
      emit_line("if (%s) then", fortran_cmp_str(inst));
      inc_indent();
      emit_line("pc = %s - 1", value_str(&inst->jmp));
      dec_indent();
      emit_line("end if");
      break;

    default:
      error("oops");
  }
}

void target_f90(Module* module) {
  init_state_f90(module->data);

  int num_funcs = emit_chunked_main_loop(module->text,
                                         f90_emit_func_prologue,
                                         f90_emit_func_epilogue,
                                         f90_emit_pc_change,
                                         f90_emit_inst);

  emit_line("");
  emit_line("program elvm");
  inc_indent();
  emit_line("use data");
  emit_line("implicit none");

  emit_line("");
  emit_line("call init");
  emit_line("do while (.true.)");
  inc_indent();

  emit_line("select case (pc / %d)", CHUNKED_FUNC_SIZE);
  inc_indent();

  for (int i = 0; i < num_funcs; i++) {
    emit_line("case (%d)", i);
    inc_indent();
    emit_line("call func%d", i);
    dec_indent();
  }

  dec_indent();
  emit_line("end select");

  dec_indent();
  emit_line("end do");

  dec_indent();
  emit_line("end program elvm");
}
