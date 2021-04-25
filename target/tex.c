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


static void tex_emit_inst(Inst* inst) {
  switch (inst->op) {
  case MOV:
    emit_line("\\@op@mov\\@reg@%s{%s}%%",
              reg_names[inst->dst.reg], tex_src_str(inst));
    break;

  case ADD:
    emit_line("\\@op@add\\@reg@%s{%s}%%",
              reg_names[inst->dst.reg], tex_src_str(inst));
    break;

  case SUB:
    emit_line("\\@op@sub\\@reg@%s{%s}%%",
              reg_names[inst->dst.reg], tex_src_str(inst));
    break;

  case LOAD:
    emit_line("\\@op@load\\@reg@%s{%s}%%",
              reg_names[inst->dst.reg], tex_src_str(inst));
    break;

  case STORE:
    emit_line("\\@op@store\\@reg@%s{%s}%%",
              reg_names[inst->dst.reg], tex_src_str(inst));
    break;

  case PUTC:
    emit_line("\\@op@putc{%s}%%", tex_src_str(inst));
    break;

  case GETC:
    emit_line("\\@op@getc\\@reg@%s", reg_names[inst->dst.reg]);
    break;

  case EXIT:
    emit_line("\\@op@exit");
    break;

  case DUMP:
    break;

  case EQ:
    emit_line("\\@op@eq\\@reg@%s{%s}%%",
              reg_names[inst->dst.reg],
              tex_src_str(inst));
    break;

  case LT:
    emit_line("\\@op@lt\\@reg@%s{%s}%%",
              reg_names[inst->dst.reg],
              tex_src_str(inst));
    break;

  case GT:
    emit_line("\\@op@gt\\@reg@%s{%s}%%",
              reg_names[inst->dst.reg],
              tex_src_str(inst));
    break;

  case NE:
    emit_line("\\@op@ne\\@reg@%s{%s}%%",
              reg_names[inst->dst.reg],
              tex_src_str(inst));
    break;

  case LE:
    emit_line("\\@op@le\\@reg@%s{%s}%%",
              reg_names[inst->dst.reg],
              tex_src_str(inst));
    break;

  case GE:
    emit_line("\\@op@ge\\@reg@%s{%s}%%",
              reg_names[inst->dst.reg],
              tex_src_str(inst));
    break;

  case JEQ:
    emit_line("\\@op@jeq\\@reg@%s{%s}{%s}%%",
              reg_names[inst->dst.reg],
              tex_src_str(inst),
              tex_value_str(&inst->jmp));
    break;

  case JLT:
    emit_line("\\@op@jlt\\@reg@%s{%s}{%s}%%",
              reg_names[inst->dst.reg],
              tex_src_str(inst),
              tex_value_str(&inst->jmp));
    break;

  case JGT:
    emit_line("\\@op@jgt\\@reg@%s{%s}{%s}%%",
              reg_names[inst->dst.reg],
              tex_src_str(inst),
              tex_value_str(&inst->jmp));
    break;

  case JNE:
    emit_line("\\@op@jne\\@reg@%s{%s}{%s}%%",
              reg_names[inst->dst.reg],
              tex_src_str(inst),
              tex_value_str(&inst->jmp));
    break;

  case JLE:
    emit_line("\\@op@jle\\@reg@%s{%s}{%s}%%",
              reg_names[inst->dst.reg],
              tex_src_str(inst),
              tex_value_str(&inst->jmp));
    break;

  case JGE:
    emit_line("\\@op@jge\\@reg@%s{%s}{%s}%%",
              reg_names[inst->dst.reg],
              tex_src_str(inst),
              tex_value_str(&inst->jmp));
    break;

  case JMP:
    emit_line("\\@op@jmp{%s}%%", tex_value_str(&inst->jmp));
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

  // Define macros for EIR's operations
  emit_line("\\def\\@op@mov#1#2{\\edef#1{#2}}");
  emit_line("\\def\\@op@add#1#2{"
            "\\count0=#1\\relax\\advance\\count0by#2\\relax"
            "\\ifnum\\count0>%d\\advance\\count0by-%s\\fi"
            "\\edef#1{\\the\\count0}}",
            UINT_MAX,
            "16777216");
  emit_line("\\def\\@op@sub#1#2{"
            "\\count0=#1\\relax\\advance\\count0by-#2\\relax"
            "\\ifnum\\count0<0\\advance\\count0by%s\\fi"
            "\\edef#1{\\the\\count0}}", "16777216");
  emit_line("\\def\\@op@load#1#2{"
            "\\edef#1{"
            "\\expandafter\\ifx\\csname @mem@#2\\endcsname\\relax0"
            "\\else\\csname @mem@#2\\endcsname\\fi}}");
  emit_line("\\def\\@op@store#1#2{"
            "\\expandafter\\let\\csname @mem@#2\\endcsname#1}");
  emit_line("\\def\\@op@putc#1{\\immediate\\write\\@out{#1}}");
  emit_line("\\def\\@op@getc#1{"
            "\\read-1to\\@temp\\count0=\\@temp\\edef#1{\\the\\count0}}");
  emit_line("\\def\\@op@exit{\\let\\@@next\\relax}");
  emit_line("\\def\\@op@eq#1#2{\\ifnum#1=#2\\edef#1{1}\\else\\edef#1{0}\\fi}");
  emit_line("\\def\\@op@lt#1#2{\\ifnum#1<#2\\edef#1{1}\\else\\edef#1{0}\\fi}");
  emit_line("\\def\\@op@gt#1#2{\\ifnum#1>#2\\edef#1{1}\\else\\edef#1{0}\\fi}");
  emit_line("\\def\\@op@ne#1#2{\\ifnum#1=#2\\edef#1{0}\\else\\edef#1{1}\\fi}");
  emit_line("\\def\\@op@le#1#2{"
            "\\ifnum#1=#2\\edef#1{1}"
            "\\else\\ifnum#1<#2\\edef#1{1}"
            "\\else\\edef#1{0}\\fi\\fi}");
  emit_line("\\def\\@op@ge#1#2{"
            "\\ifnum#1=#2\\edef#1{1}"
            "\\else\\ifnum#1>#2\\edef#1{1}"
            "\\else\\edef#1{0}\\fi\\fi}");
  emit_line("\\def\\@op@jmp@common#1{"
            "\\count0=#1\\relax\\advance\\count0by-1\\relax"
            "\\edef\\@reg@pc{\\the\\count0}}");
  emit_line("\\def\\@op@jeq#1#2#3{"
            "\\ifnum#1=#2\\relax\\@op@jmp@common{#3}\\fi}");
  emit_line("\\def\\@op@jlt#1#2#3{"
            "\\ifnum#1<#2\\relax\\@op@jmp@common{#3}\\fi}");
  emit_line("\\def\\@op@jgt#1#2#3{"
            "\\ifnum#1>#2\\relax\\@op@jmp@common{#3}\\fi}");
  emit_line("\\def\\@op@jne#1#2#3{"
            "\\ifnum#1=#2\\else\\relax\\@op@jmp@common{#3}\\fi}");
  emit_line("\\def\\@op@jle#1#2#3{"
            "\\ifnum#1=#2\\relax\\@op@jmp@common{#3}"
            "\\else\\ifnum#1<#2\\relax\\@op@jmp@common{#3}\\fi\\fi}");
  emit_line("\\def\\@op@jge#1#2#3{"
            "\\ifnum#1=#2\\relax\\@op@jmp@common{#3}"
            "\\else\\ifnum#1>#2\\relax\\@op@jmp@common{#3}\\fi\\fi}");
  emit_line("\\let\\@op@jmp\\@op@jmp@common");

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
