#include <assert.h>

#include <ir/ir.h>
#include <target/util.h>

static void init_state_tf(Data* data) {
  emit_line("import sys");
  emit_line("import tensorflow as tf");
  emit_line("from tensorflow.python.framework import function");
  emit_line("from tensorflow.python.framework import tensor_shape");
  emit_line("from tensorflow.python.ops import inplace_ops");
  emit_line("");

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
  emit_line("mem = tf.concat([data, "
            "tf.zeros([(1<<24)-len(data)], dtype=tf.int32)], 0, name='mem')");
  emit_line("done = tf.constant(0, name='done')");
  emit_line("out = tf.constant('', name='out')");
  emit_line("CHAR_TBL = tf.constant([chr(i) for i in range(256)])");
  emit_line("input = list(map(ord, sys.stdin.read()))");
  emit_line("INPUT_LEN = len(input)");
  emit_line("INPUT = tf.placeholder("
            "shape=[INPUT_LEN], dtype=tf.int32)");
  emit_line("inp = tf.constant(-1, name='inp')");
  emit_line("");
  emit_line("def read_input(inp):");
  inc_indent();
  emit_line("global INPUT");
  emit_line("global INPUT_LEN");
  emit_line("inp = inp + tf.constant(1)");
  emit_line("r = tf.cond(tf.less(inp, INPUT_LEN),"
            " lambda: INPUT[inp], lambda: tf.constant(0))");
  emit_line("r.set_shape(())");
  emit_line("return r, inp");
  dec_indent();

  emit_line("");
  emit_line("@function.Defun(tf.int32, tf.int32)");
  emit_line("def elvm_add(x, y):");
  emit_line("  return (x + y) %% 16777216");
  emit_line("");
  emit_line("@function.Defun(tf.int32, tf.int32)");
  emit_line("def elvm_sub(x, y):");
  emit_line("  return (x - y + 16777216) %% 16777216");
}

static const char* tf_value_str(Value* v) {
  if (v->type == REG) {
    return reg_names[v->reg];
  } else if (v->type == IMM) {
    return format("tf.constant(%d)", v->imm);
  } else {
    error("invalid value");
  }
}

static const char* tf_src_str(Inst* inst) {
  return tf_value_str(&inst->src);
}

static const char* tf_cmp(Inst* inst) {
  uint op = normalize_cond(inst->op, false);
  static const char* OPS[] = {
    "equal", "not_equal", "less", "greater", "less_equal", "greater_equal"
  };
  return format("tf.%s(%s, %s)",
                OPS[op - JEQ], tf_value_str(&inst->dst), tf_src_str(inst));
}

static void tf_emit_inst(Inst* inst) {
  switch (inst->op) {
  case MOV:
    emit_line("%s = %s", reg_names[inst->dst.reg], tf_src_str(inst));
    break;

  case ADD:
    emit_line("%s = elvm_add(%s, %s)",
              reg_names[inst->dst.reg],
              reg_names[inst->dst.reg], tf_src_str(inst));
    emit_line("%s.set_shape(())", reg_names[inst->dst.reg]);
    break;

  case SUB:
    emit_line("%s = elvm_sub(%s, %s)",
              reg_names[inst->dst.reg],
              reg_names[inst->dst.reg], tf_src_str(inst));
    emit_line("%s.set_shape(())", reg_names[inst->dst.reg]);
    break;

  case LOAD:
    emit_line("%s = mem[%s]", reg_names[inst->dst.reg], tf_src_str(inst));
    // Workaround for .data, but I'm not sure why we need this.
    // TODO: Remove this.
    emit_line("%s.set_shape(())", reg_names[inst->dst.reg]);
    break;

  case STORE:
    emit_line("mem = inplace_ops.inplace_update(mem, %s, %s)",
              tf_src_str(inst), reg_names[inst->dst.reg]);
    break;

  case PUTC:
    emit_line("out = out + CHAR_TBL[tf.mod(%s, 256)]", tf_src_str(inst));
    emit_line("out.set_shape(())");
    break;

  case GETC:
    emit_line("%s, inp = read_input(inp)",
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
    emit_line("%s = tf.cond(%s,"
              " lambda: tf.constant(1), lambda: tf.constant(0))",
              reg_names[inst->dst.reg], tf_cmp(inst));
    break;

  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
    emit_line("pc = tf.cond(%s, lambda: %s - 1, lambda: pc)",
              tf_cmp(inst), tf_value_str(&inst->jmp));
    break;

  case JMP:
    emit_line("pc = %s - 1", tf_value_str(&inst->jmp));
    break;

  default:
    error("oops");
  }
}

static void tf_emit_func_epilogue(const char* args) {
  emit_line("pc = pc + 1");
  emit_line("return [%s]", args);
  dec_indent();
}

static void tf_emit_pc_dispatch(int min_pc, int max_pc,
                                const char* state_args_str) {
  assert(min_pc <= max_pc);
  if (min_pc == max_pc) {
    emit_line("lambda: pc_%d(%s),", min_pc, state_args_str);
    return;
  }
  int mid_pc = (min_pc + max_pc) / 2;
  emit_line("lambda: tf.cond(tf.less(pc, %d),", mid_pc + 1);
  inc_indent();
  tf_emit_pc_dispatch(min_pc, mid_pc, state_args_str);
  tf_emit_pc_dispatch(mid_pc + 1, max_pc, state_args_str);
  emit_line("),", mid_pc + 1);
  dec_indent();
}

void target_tf(Module* module) {
  init_state_tf(module->data);

  static const char STATE_ARGS_STR[] = "a,b,c,d,bp,sp,pc,done,mem,out,inp";
  int prev_pc = -1;
  for (Inst* inst = module->text; inst; inst = inst->next) {
    if (prev_pc != inst->pc) {
      if (inst->pc) {
        tf_emit_func_epilogue(STATE_ARGS_STR);
      }
      emit_line("");
      emit_line("def pc_%d(%s):", inst->pc, STATE_ARGS_STR);
      inc_indent();
      emit_line("global CHAR_TBL");
    }
    prev_pc = inst->pc;
    tf_emit_inst(inst);
  }
  tf_emit_func_epilogue(STATE_ARGS_STR);

  emit_line("");
  emit_line("def run_step(%s):", STATE_ARGS_STR);
  inc_indent();
  emit_line("r = (");
  inc_indent();
  tf_emit_pc_dispatch(0, prev_pc, STATE_ARGS_STR);
  dec_indent();
  emit_line(")");
  emit_line("r = r[0]()");
  emit_line("r[8].set_shape([1<<24])");
  emit_line("return r");
  dec_indent();

  emit_line("");
  emit_line("loop = tf.while_loop(");
  inc_indent();
  emit_line("lambda %s: tf.equal(done, 0),", STATE_ARGS_STR);
  emit_line("run_step,");
  emit_line("loop_vars=[%s])", STATE_ARGS_STR);
  dec_indent();

  emit_line("");
  emit_line("sess = tf.Session()");
  emit_line("tf.train.write_graph(loop[9].graph.as_graph_def(),"
            " '/tmp', 'graph.pbtxt')");
  emit_line("sess.run(tf.global_variables_initializer())");
  emit_line("r = sess.run(loop, feed_dict={INPUT: input})");

  emit_line("");
  emit_line("sys.stdout.write(r[9])");
}
