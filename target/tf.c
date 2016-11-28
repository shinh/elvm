#include <ir/ir.h>
#include <target/util.h>

static void init_state_tf(Data* data) {
  emit_line("import tensorflow as tf");
  emit_line("from tensorflow.python.framework import tensor_shape");
  for (int i = 0; i < 7; i++) {
    //emit_line("%s = tf.Variable(0, name='%s')", reg_names[i], reg_names[i]);
    emit_line("%s = tf.constant(0, name='%s')", reg_names[i], reg_names[i]);
  }
  //emit_line("mem = tf.Variable([0] * (1<<24), dtype=tf.int32, name='mem')");
  //emit_line("mem = tf.zeros([1<<24], dtype=tf.int32, name='mem')");
  emit_line("data = []");
  for (int mp = 0; data; data = data->next, mp++) {
    emit_line("data.append(%d)", data->v);
  }
  emit_line("mem = tf.concat(0, [data, "
            "tf.zeros([(1<<24)-len(data)], dtype=tf.int32, name='mem')])");
  emit_line("done = tf.constant(0, name='done')");
}

#if 0
static void tf_emit_func_prologue(int func_id) {
  emit_line("");
  emit_line("def func%d():", func_id);
  inc_indent();
  for (int i = 0; i < 7; i++) {
    emit_line("global %s", reg_names[i]);
  }
  emit_line("global mem");
  emit_line("");

  emit_line("while %d <= pc and pc < %d:",
            func_id * CHUNKED_FUNC_SIZE, (func_id + 1) * CHUNKED_FUNC_SIZE);
  inc_indent();
  emit_line("if False:");
  inc_indent();
  emit_line("pass");
}

static void tf_emit_func_epilogue(void) {
  dec_indent();
  emit_line("pc += 1");
  dec_indent();
  dec_indent();
}

static void tf_emit_pc_change(int pc) {
  emit_line("");
  dec_indent();
  emit_line("elif pc == %d:", pc);
  inc_indent();
}
#endif

static const char* tf_value_str(Value* v) {
  if (v->type == REG) {
    return reg_names[v->reg];
  } else if (v->type == IMM) {
    return format("tf.constant(%d)", v->imm);
  } else {
    error("invalid value");
  }
}

static void tf_emit_inst(Inst* inst) {
  switch (inst->op) {
  case MOV:
    emit_line("%s = %s", reg_names[inst->dst.reg], src_str(inst));
    break;

  case ADD:
    emit_line("%s = (%s + %s) & " UINT_MAX_STR,
              reg_names[inst->dst.reg],
              reg_names[inst->dst.reg], src_str(inst));
    break;

  case SUB:
    emit_line("%s = (%s - %s) & " UINT_MAX_STR,
              reg_names[inst->dst.reg],
              reg_names[inst->dst.reg], src_str(inst));
    break;

  case LOAD:
    emit_line("%s = mem[%s]", reg_names[inst->dst.reg], src_str(inst));
    break;

  case STORE:
    emit_line("mem[%s] = %s", src_str(inst), reg_names[inst->dst.reg]);
    break;

  case PUTC:
    emit_line("sys.stdout.write(chr(%s))", src_str(inst));
    break;

  case GETC:
    emit_line("_ = sys.stdin.read(1); %s = ord(_) if _ else 0",
              reg_names[inst->dst.reg]);
    break;

  case EXIT:
    emit_line("done = tf.constant(1)");
    break;

  case DUMP:
    break;

  case EQ:
  case NE:
  case LT:
  case GT:
  case LE:
  case GE:
    emit_line("%s = int(%s)",
              reg_names[inst->dst.reg], cmp_str(inst, "True"));
    break;

  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
    error("TODO");

  case JMP:
    emit_line("pc = %s - 1", tf_value_str(&inst->jmp));
    break;

  default:
    error("oops");
  }
}

void target_tf(Module* module) {
  init_state_tf(module->data);

  static const char STATE_ARGS_STR[] = "a,b,c,d,pc,done,mem";
  int prev_pc = -1;
  for (Inst* inst = module->text; inst; inst = inst->next) {
    if (prev_pc != inst->pc) {
      if (inst->pc) {
        emit_line("pc = pc + 1");
        emit_line("return [%s]", STATE_ARGS_STR);
        dec_indent();
      }
      emit_line("");
      emit_line("def pc_%x(%s):", inst->pc, STATE_ARGS_STR);
      inc_indent();
    }
    prev_pc = inst->pc;
    tf_emit_inst(inst);
  }
  emit_line("pc = pc + 1");
  emit_line("return [%s]", STATE_ARGS_STR);
  dec_indent();

  emit_line("");
  emit_line("def run_step(%s):", STATE_ARGS_STR);
  inc_indent();
  emit_line("fn_pairs = []");
  for (int i = 0; i < prev_pc; i++) {
    emit_line("fn_pairs.append((tf.equal(pc, %d), lambda: pc_%d(%s)))",
              i, i, STATE_ARGS_STR);
  }
  emit_line("r = tf.case(fn_pairs, lambda: pc_%d(%s))",
            prev_pc, STATE_ARGS_STR);
  emit_line("r[6].set_shape([1<<24])");
  emit_line("return r");
  dec_indent();

  emit_line("");
  emit_line("tf.while_loop(");
  inc_indent();
  emit_line("lambda %s: tf.not_equal(done, 0),", STATE_ARGS_STR);
  emit_line("run_step,");
  emit_line("loop_vars=[%s])", STATE_ARGS_STR);
  dec_indent();
}
