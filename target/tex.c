#include <ir/ir.h>
#include <target/util.h>

const char* tex_value_str(Value* v) {
  if (v->type == REG) {
    return format("\\@reg@%s", reg_names[v->reg]);
  } else if (v->type == IMM) {
    return format("%d", v->imm);
  } else {
    error("invalid value");
  }
}

static const char* tex_src_str(Inst* inst) {
  return tex_value_str(&inst->src);
}

static const char* tex_cmp_op_str(Inst* inst) {
  int op = normalize_cond(inst->op, 0);
  switch (op) {
    case JEQ:
      return "=";
    case JLE:
    case JLT:
      return "<";
    case JGE:
    case JGT:
      return ">";
    default:
      error("oops");
  }
}

static void tex_emit_inst(Inst* inst) {
  switch (inst->op) {
  case MOV:
    emit_line("\\edef\\@reg@%s{%s}%%", reg_names[inst->dst.reg], tex_src_str(inst));
    break;

  case ADD:
    emit_line("\\count0=\\@reg@%s\\relax", reg_names[inst->dst.reg]);
    emit_line("\\advance\\count0by%s\\relax", tex_src_str(inst));
    emit_line("\\ifnum\\count0>%d\\advance\\count0by-%s\\fi", UINT_MAX, "16777216");
    emit_line("\\edef\\@reg@%s{\\the\\count0}%%", reg_names[inst->dst.reg]);
    break;

  case SUB:
    emit_line("\\count0=\\@reg@%s\\relax", reg_names[inst->dst.reg]);
    emit_line("\\advance\\count0by-%s\\relax", tex_src_str(inst));
    emit_line("\\ifnum\\count0<0\\advance\\count0by%s\\fi", "16777216");
    emit_line("\\edef\\@reg@%s{\\the\\count0}%%", reg_names[inst->dst.reg]);
    break;

  case LOAD:
    emit_line("\\def\\@reg@%s{0}%%", reg_names[inst->dst.reg]);
    emit_line("\\expandafter\\ifx\\csname @mem@%s\\endcsname\\relax\\else\\edef\\@reg@%s{\\csname @mem@%s\\endcsname}\\fi",
              tex_src_str(inst), reg_names[inst->dst.reg], tex_src_str(inst));
    break;

  case STORE:
    emit_line("\\expandafter\\let\\csname @mem@%s\\endcsname\\@reg@%s",
              tex_src_str(inst), reg_names[inst->dst.reg]);
    break;

  case PUTC:
    // a output char c is represented by C where C is the code of c.
    emit_line("\\immediate\\write\\@out{%s}%%", tex_src_str(inst));
    break;

  case GETC:
    // input is also...
    emit_line("\\read-1to\\@temp\\count0=\\@temp\\edef\\@reg@%s{\\the\\count0}%%", reg_names[inst->dst.reg]);
    break;

  case EXIT:
    emit_line("\\let\\@@next\\relax");
    break;

  case DUMP:
    break;

  case EQ:
  case LT:
  case GT:
    emit_line("\\ifnum\\@reg@%s%s%s\\edef\\@reg@%s{1}\\else\\edef\\@reg@%s{0}\\fi%%",
              reg_names[inst->dst.reg],
              tex_cmp_op_str(inst),
              tex_src_str(inst),
              reg_names[inst->dst.reg],
              reg_names[inst->dst.reg]);
    break;
  case NE:
    emit_line("\\ifnum\\@reg@%s=%s\\edef\\@reg@%s{0}\\else\\edef\\@reg@%s{1}\\fi%%",
              reg_names[inst->dst.reg], tex_src_str(inst),
              reg_names[inst->dst.reg], reg_names[inst->dst.reg]);
    break;
  case LE:
  case GE:
    emit_line("\\ifnum\\@reg@%s=%s\\edef\\@reg@%s{1}\\else"
              "\\ifnum\\@reg@%s%s%s\\edef\\@reg@%s{1}\\else\\edef\\@reg@%s{0}\\fi\\fi%%",
              reg_names[inst->dst.reg], tex_src_str(inst),
              reg_names[inst->dst.reg],
              reg_names[inst->dst.reg], tex_cmp_op_str(inst), tex_src_str(inst),
              reg_names[inst->dst.reg],
              reg_names[inst->dst.reg]);
    break;

  case JEQ:
  case JLT:
  case JGT:
    emit_line("\\count0=%s\\relax\\advance\\count0by-1\\relax", tex_value_str(&inst->jmp));
    emit_line("\\ifnum\\@reg@%s%s%s\\edef\\@reg@pc{\\the\\count0}\\fi",
              reg_names[inst->dst.reg],
              tex_cmp_op_str(inst),
              tex_src_str(inst));
    break;
  case JNE:
    emit_line("\\count0=%s\\relax\\advance\\count0by-1\\relax", tex_value_str(&inst->jmp));
    emit_line("\\ifnum\\@reg@%s=%s\\else\\edef\\@reg@pc{\\the\\count0}\\fi",
              reg_names[inst->dst.reg],
              tex_src_str(inst));
    break;
  case JLE:
  case JGE:
    emit_line("\\count0=%s\\relax\\advance\\count0by-1\\relax", tex_value_str(&inst->jmp));
    emit_line("\\ifnum\\@reg@%s=%s\\edef\\@reg@pc{\\the\\count0}\\fi",
              reg_names[inst->dst.reg],
              tex_src_str(inst));
    emit_line("\\ifnum\\@reg@%s%s%s\\edef\\@reg@pc{\\the\\count0}\\fi",
              reg_names[inst->dst.reg],
              tex_cmp_op_str(inst),
              tex_src_str(inst));
    break;
  case JMP:
    emit_line("\\count0=%s\\relax\\advance\\count0by-1\\relax", tex_value_str(&inst->jmp));
    emit_line("\\edef\\@reg@pc{\\the\\count0}%%");
    break;

  default:
    error("oops");
  }
}

static void tex_init_state(Data* data) {
  // register
  for (int i = 0; i < 7; i++) {
    emit_line("\\def\\@reg@%s{0}", reg_names[i]);
  }

  // memory
  for (int mp = 0; data; data = data->next, mp++) {
    if(data->v != 0) {
      emit_line("\\expandafter\\def\\csname @mem@%d\\endcsname{%d}", mp, data->v);
    }
  }
  return;
}

void target_tex(Module* module) {
  emit_line("\\catcode`\\@=11\\relax");

  // Initialize register and memory.
  tex_init_state(module->data);

  // open a file to output
  //emit_line("\\read-1to\\@file");
  emit_line("\\newwrite\\@out");
  //emit_line("\\immediate\\openout\\@out=\\@file\\relax");
  emit_line("\\immediate\\openout\\@out=\\jobname.tex.elvm.out\\relax");

  // compile each instractions
  int prev_pc = -1;
  for (Inst* inst = module->text; inst; inst = inst->next) {
    if (prev_pc != inst->pc) {
      if(prev_pc != -1) {
        emit_line("}");
      }
      emit_line("\\expandafter\\def\\csname @inst@%d\\endcsname{%%", inst->pc);
    }
    prev_pc = inst->pc;
    tex_emit_inst(inst);
  }
  if(prev_pc != -1) {
    emit_line("}");
  }

  // define and do infinite loop
  emit_line("\\def\\@loop@main{%%");
  emit_line("\\let\\@@next\\@loop@main");
  // execute instraction
  emit_line("\\csname @inst@\\@reg@pc\\endcsname");
  // increment pc
  emit_line("\\count0=\\@reg@pc\\relax");
  emit_line("\\advance\\count0by1\\relax");
  emit_line("\\edef\\@reg@pc{\\the\\count0}%%");
  emit_line("\\@@next}\\@loop@main");

  // finnaly, close the output
  emit_line("\\immediate\\closeout\\@out");
  emit_line("\\bye");
}
