#include <ir/ir.h>
#include <target/util.h>

static int func_idx;
static int case_idx;
static int case_pc[512];
static int case_label[512];
static int putc_idx;

static void ll_init_state(void) {
  func_idx = 1;
  case_idx = 0;
  putc_idx = 0;
  for (int i = 0; i < 7; i++) {
    emit_line("@%s = common global i32 0, align 4", reg_names[i]);
  }
  emit_line("@mem = common global [16777216 x i32] zeroinitializer, align 16");
}

static void ll_emit_func_prologue(int func_id) {
  emit_line("");
  emit_line("define void @func%d() {", func_id);
  inc_indent();

  emit_line("br label %%1");

  emit_line("");
  emit_line("; <label>:1");
  emit_line("%%2 = load i32, i32* @pc, align 4");
  emit_line("%%3 = icmp ule i32 %d, %%2", func_id * CHUNKED_FUNC_SIZE);
  emit_line("br i1 %%3, label %%4, label %%7");

  emit_line("");
  emit_line("; <label>:4");
  emit_line("%%5 = load i32, i32* @pc, align 4");
  emit_line("%%6 = icmp ult i32 %%5, %d", (func_id + 1) * CHUNKED_FUNC_SIZE);
  emit_line("br label %%7");

  emit_line("");
  emit_line("; <label>:7");
  emit_line("%%8 = phi i1 [ false, %%1 ], [ %%6, %%4 ]");
  emit_line("br i1 %%8, label %%switch_top, label %%func_bottom");

  emit_line("");
  emit_line("; <label>:9");
  /* set func_idx */
  func_idx = 10;
}

static void ll_emit_func_epilogue(void) {
  emit_line("br label %%case_bottom");
  emit_line("");
  emit_line("switch_top:");
  emit_line("%%%d = load i32, i32* @pc, align 4", func_idx);
  emit_line("switch i32 %%%d, label %%case_bottom [", func_idx);
  inc_indent();
  emit_line("i32 -1, label %%9");
  for (int i=0; i<case_idx; i++) {
    emit_line("i32 %d, label %%%d", case_pc[i], case_label[i]);
  }
  dec_indent();
  emit_line("]");

  emit_line("");
  emit_line("case_bottom:");
  emit_line("%%%d = load i32, i32* @pc, align 4", func_idx+1);
  emit_line("%%%d = add i32 %%%d, 1", func_idx+2, func_idx+1);
  emit_line("store i32 %%%d, i32* @pc, align 4", func_idx+2);
  emit_line("br label %%1");

  emit_line("");
  emit_line("func_bottom:");
  emit_line("ret void");
  dec_indent();
  emit_line("}");
  /* reset func_idx */
  func_idx = 1;
  case_idx = 0;
  putc_idx = 0;
}

static void ll_emit_pc_change(int pc) {
  emit_line("br label %%case_bottom");

  emit_line("");
  emit_line("; <label>:%d", func_idx);
  case_pc[case_idx] = pc;
  case_label[case_idx] = func_idx;
  case_idx += 1;
  func_idx += 1;
}

const char* ll_cmp_str(Inst* inst) {
  int op = normalize_cond(inst->op, 0);
  switch (op) {
    case JEQ:
      return "eq";
    case JNE:
      return "ne";
    case JLT:
      return "ult";
    case JGT:
      return "ugt";
    case JLE:
      return "ule";
    case JGE:
      return "uge";
    default:
      error("oops");
  }
}

static void ll_emit_cmp(Inst* inst) {
  if (inst->src.type == REG) {
    emit_line("%%%d = load i32, i32* @%s, align 4", func_idx, reg_names[inst->dst.reg]);
    emit_line("%%%d = load i32, i32* @%s, align 4", func_idx+1, src_str(inst));
    emit_line("%%%d = icmp %s i32 %%%d, %%%d",
              func_idx+2, ll_cmp_str(inst), func_idx, func_idx+1);
    func_idx += 3;
  } else if (inst->src.type == IMM) {
    emit_line("%%%d = load i32, i32* @%s, align 4", func_idx, reg_names[inst->dst.reg]);
    emit_line("%%%d = icmp %s i32 %%%d, %s",
              func_idx+1, ll_cmp_str(inst), func_idx, src_str(inst));
    func_idx += 2;
  } else {
    error("invalid value");
  }
}

const char* ll_emit_load(Inst* inst) {
  if (inst->src.type == REG) {
    emit_line("%%%d = load i32, i32* @%s, align 4", func_idx, reg_names[inst->dst.reg]);
    emit_line("%%%d = load i32, i32* @%s, align 4", func_idx+1, src_str(inst));
    func_idx += 2;
    return format("%d", func_idx);
  } else if (inst->src.type == IMM) {
    emit_line("%%%d = load i32, i32* @%s, align 4", func_idx, reg_names[inst->dst.reg]);
    func_idx += 1;
    return format("@%s", reg_names[inst->dst.reg]);
  } else {
    error("invalid value");
  }
}

static void ll_emit_inst(Inst* inst) {
  switch (inst->op) {
  case MOV:
    if (inst->src.type == REG) {
      emit_line("%%%d = load i32, i32* @%s, align 4", func_idx, src_str(inst));
      emit_line("store i32 %%%d, i32* @%s, align 4", func_idx, reg_names[inst->dst.reg]);
      func_idx += 1;
    } else if (inst->src.type == IMM) {
      emit_line("store i32 %s, i32* @%s, align 4", src_str(inst), reg_names[inst->dst.reg]);
    }
    break;

  case ADD:
    if (inst->src.type == REG) {
      emit_line("%%%d = load i32, i32* @%s, align 4", func_idx, reg_names[inst->dst.reg]);
      emit_line("%%%d = load i32, i32* @%s, align 4", func_idx+1, src_str(inst));
      emit_line("%%%d = add i32 %%%d, %%%d", func_idx+2, func_idx, func_idx+1);
      func_idx += 3;
    } else if (inst->src.type == IMM) {
      emit_line("%%%d = load i32, i32* @%s, align 4", func_idx, reg_names[inst->dst.reg]);
      emit_line("%%%d = add i32 %%%d, %s", func_idx+1, func_idx, src_str(inst));
      func_idx += 2;
    } else {
      error("invalid value");
    }
    emit_line("%%%d = and i32 %%%d, 16777215", func_idx, func_idx-1);
    emit_line("store i32 %%%d, i32* @%s, align 4", func_idx, reg_names[inst->dst.reg]);
    func_idx += 1;
    break;

  case SUB:
    if (inst->src.type == REG) {
      emit_line("%%%d = load i32, i32* @%s, align 4", func_idx, reg_names[inst->dst.reg]);
      emit_line("%%%d = load i32, i32* @%s, align 4", func_idx+1, src_str(inst));
      emit_line("%%%d = sub i32 %%%d, %%%d", func_idx+2, func_idx, func_idx+1);
      func_idx += 3;
    } else if (inst->src.type == IMM) {
      emit_line("%%%d = load i32, i32* @%s, align 4", func_idx, reg_names[inst->dst.reg]);
      emit_line("%%%d = sub i32 %%%d, %s", func_idx+1, func_idx, src_str(inst));
      func_idx += 2;
    } else {
      error("invalid value");
    }
    emit_line("%%%d = and i32 %%%d, 16777215", func_idx, func_idx-1);
    emit_line("store i32 %%%d, i32* @%s, align 4", func_idx, reg_names[inst->dst.reg]);
    func_idx += 1;
    break;

  case LOAD:
    if (inst->src.type == REG) {
      emit_line("%%%d = load i32, i32* @%s, align 4", func_idx, src_str(inst));
      emit_line("%%%d = zext i32 %%%d to i64", func_idx+1, func_idx);
      emit_line("%%%d = getelementptr inbounds [16777216 x i32], [16777216 x i32]* @mem, i32 0, i64 %%%d", func_idx+2, func_idx+1);
      emit_line("%%%d = load i32, i32* %%%d, align 4", func_idx+3, func_idx+2);
      emit_line("store i32 %%%d, i32* @%s, align 4", func_idx+3, reg_names[inst->dst.reg]);
      func_idx += 4;
    } else if (inst->src.type == IMM) {
      emit_line("%%%d = load i32, i32* getelementptr inbounds ([16777216 x i32], [16777216 x i32]* @mem, i32 0, i64 %s)", func_idx, src_str(inst));
      emit_line("store i32 %%%d, i32* @%s, align 4", func_idx, reg_names[inst->dst.reg]);
      func_idx += 1;
    } else {
      error("invalid value");
    }
    break;

  case STORE:
    if (inst->src.type == REG) {
      emit_line("%%%d = load i32, i32* @%s, align 4", func_idx, reg_names[inst->dst.reg]);
      emit_line("%%%d = load i32, i32* @%s, align 4", func_idx+1, src_str(inst));
      emit_line("%%%d = zext i32 %%%d to i64", func_idx+2, func_idx+1);
      emit_line("%%%d = getelementptr inbounds [16777216 x i32], [16777216 x i32]* @mem, i32 0, i64 %%%d", func_idx+3, func_idx+2);
      emit_line("store i32 %%%d, i32* %%%d, align 4", func_idx, func_idx+3);
      func_idx += 4;
    } else if (inst->src.type == IMM) {
      emit_line("%%%d = load i32, i32* @%s, align 4", func_idx, reg_names[inst->dst.reg]);
      emit_line("store i32 %%%d, i32* getelementptr inbounds ([16777216 x i32], [16777216 x i32]* @mem, i32 0, i64 %s)", func_idx, src_str(inst));
      func_idx += 1;
    } else {
      error("invalid value");
    }
    break;

  case PUTC:
    if (inst->src.type == REG) {
      emit_line("%%%d = load i32, i32* @%s, align 4", func_idx, src_str(inst));
      emit_line("%%%d = call i32 @putchar(i32 %%%d)", func_idx+1, func_idx);
      func_idx += 2;
    } else if (inst->src.type == IMM) {
      emit_line("%%%d = call i32 @putchar(i32 %s)", func_idx, src_str(inst));
      func_idx += 1;
    } else {
      error("invalid value");
    }
    break;

  case GETC:
    emit_line("%%_%d = alloca i32, align 4", putc_idx);
    emit_line("%%%d = call i32 @getchar()", func_idx);
    emit_line("store i32 %%%d, i32* %%_%d, align 4", func_idx, putc_idx);
    emit_line("%%%d = load i32, i32* %%_%d, align 4", func_idx+1, putc_idx);
    emit_line("%%%d = icmp ne i32 %%%d, -1", func_idx+2, func_idx+1);
    emit_line("br i1 %%%d, label %%%d, label %%%d", func_idx+2, func_idx+3, func_idx+5);

    emit_line("");
    emit_line("; <label>:%%%d", func_idx+3);
    emit_line("%%%d = load i32, i32* %%_%d, align 4", func_idx+4, putc_idx);
    emit_line("br label %%%d", func_idx+6);

    emit_line("");
    emit_line("; <label>:%%%d", func_idx+5);
    emit_line("br label %%%d", func_idx+6);

    emit_line("");
    emit_line("; <label>:%%%d", func_idx+6);
    emit_line("%%%d = phi i32 [ %%%d, %%%d ], [0, %%%d]",
                func_idx+7, func_idx+4, func_idx+3, func_idx+5);
    emit_line("store i32 %%%d, i32* @%s, align 4", func_idx+7, reg_names[inst->dst.reg]);
    func_idx += 8;
    putc_idx += 1;
    break;

  case EXIT:
    emit_line("call void @exit(i32 0)");
    break;

  case DUMP:
    break;

  case EQ:
  case NE:
  case LT:
  case GT:
  case LE:
  case GE:
    ll_emit_cmp(inst);
    emit_line("%%%d = zext i1 %%%d to i32", func_idx, func_idx-1);
    emit_line("store i32 %%%d, i32* @%s, align 4", func_idx, reg_names[inst->dst.reg]);
    func_idx += 1;
    break;

  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
    emit_line("; cmp jmp %s", ll_cmp_str(inst));
    ll_emit_cmp(inst);
    if (inst->jmp.type == REG) {
      emit_line("br i1 %%%d, label %%%d, label %%%d",
                func_idx-1, func_idx, func_idx+3);
      emit_line("");
      emit_line("; <label>:%%%d", func_idx);
      emit_line("%%%d = load i32, i32* @%s, align 4", func_idx+1, value_str(&inst->jmp));
      emit_line("%%%d = sub i32 %%%d, 1", func_idx+2, func_idx+1);
      emit_line("store i32 %d, i32* @pc, align 4", func_idx+2);
      func_idx += 3;
    } else if (inst->jmp.type == IMM) {
      emit_line("br i1 %%%d, label %%%d, label %%%d",
                func_idx-1, func_idx, func_idx+1);
      emit_line("");
      emit_line("; <label>:%%%d", func_idx);
      emit_line("store i32 %d, i32* @pc, align 4", inst->jmp.imm-1);
      func_idx += 1;
    } else {
      error("invalid value");
    }
    emit_line("br label %%%d", func_idx);
    emit_line("");
    emit_line("; <label>:%%%d", func_idx);
    func_idx += 1;
    break;

  case JMP:
    emit_line("; jmp");
    if (inst->jmp.type == REG) {
      emit_line("%%%d = load i32, i32* @%s, align 4", func_idx, value_str(&inst->jmp));
      emit_line("%%%d = sub i32 %%%d, 1", func_idx+1, func_idx);
      emit_line("store i32 %%%d, i32* @pc, align 4", func_idx+1);
      func_idx += 2;
    } else if (inst->jmp.type == IMM) {
      emit_line("store i32 %d, i32* @pc, align 4", inst->jmp.imm-1);
    }
    break;

  default:
    error("oops");
  }
}

void target_ll(Module* module) {
  ll_init_state();

  int num_funcs = emit_chunked_main_loop(module->text,
                                         ll_emit_func_prologue,
                                         ll_emit_func_epilogue,
                                         ll_emit_pc_change,
                                         ll_emit_inst);

  emit_line("");
  emit_line("declare i32 @getchar()");
  emit_line("declare i32 @putchar(i32)");
  emit_line("declare void @exit(i32)");

  emit_line("");
  emit_line("define i32 @main() {");
  inc_indent();

  emit_line("%%1 = alloca i32, align 4");
  emit_line("store i32 0, i32* %%1");
  Data* data = module->data;
  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      emit_line("store i32 %d, i32* getelementptr inbounds ([16777216 x i32], [16777216 x i32]* @mem, i32 0, i64 %d), align 4", data->v, mp);
    }
  }
  emit_line("br label %%2");

  emit_line("");
  emit_line("; <label>:2");
  emit_line("%%3 = load i32, i32* @pc, align 4");
  emit_line("%%4 = udiv i32 %%3, %d", CHUNKED_FUNC_SIZE);

  int label_offset = 5;
  int while_bottom = label_offset + num_funcs;

  emit_line("switch i32 %%4, label %%%d [", while_bottom);
  for (int i = 0; i < num_funcs; i++) {
    emit_line("i32 %d, label %%%d", i, label_offset+i);
  }
  emit_line("]");

  emit_line("");
  for (int i = 0; i < num_funcs; i++) {
    emit_line("; <label>:%d", label_offset+i);
    inc_indent();
    emit_line("call void @func%d()", i);
    emit_line("br label %%%d", while_bottom);
    dec_indent();
  }

  emit_line("");
  emit_line("; <label>:%d", while_bottom);
  emit_line("br label %%2");

  emit_line("");
  emit_line("%%%d = load i32, i32* %%1, align 4", while_bottom+2);
  emit_line("ret i32 %%%d", while_bottom+2);
  dec_indent();
  emit_line("}");
}
