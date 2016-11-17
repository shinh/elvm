#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include <ir/ir.h>
#include <target/util.h>

static const char SED_REG_NAMES[] = "ABCDFS";

static void sed_init_state(Data* data) {
  emit_line(":in_loop");
  emit_line("/^$/{x\ns/$/a,/\nx\nbin_done\n}");
  for (int i = 1; i < 128; i++) {
    if (i == 10)
      continue;
    putchar('/');
    putchar('^');
    if (i == '$' || i == '.' || i == '/' ||
        i == '[' || i == '\\' || i == ']') {
      putchar('\\');
    }
    putchar(i);
    emit_line("/{s/.//\nx\ns/$/%x,/\nx\nbin_loop\n}", i);
  }
  emit_line(":in_done");
  emit_line("${");

  emit_line("x");
  emit_line("s/a,$//");
  emit_line("s/.*/i=& /");
  for (int i = 0; i < 6; i++) {
    emit_line("s/$/%c=0 /", SED_REG_NAMES[i]);
  }
  emit_line("s/$/o= /");
  for (int mp = 0; data; data = data->next, mp++) {
    emit_line("s/$/m%x=%x /", mp, data->v);
  }
  emit_line("x");
}

static void sed_emit_value(Value* v) {
  if (v->type == REG) {
    emit_line("G");
    emit_line("s/\\n[^%c]* %c=\\([^ ]*\\).*/\\1/",
              SED_REG_NAMES[v->reg], SED_REG_NAMES[v->reg]);
  } else {
    emit_line("s/$/%x/", v->imm);
  }
}

static void sed_emit_src(Inst* inst) {
  sed_emit_value(&inst->src);
}

static void sed_emit_dst(Inst* inst) {
  sed_emit_value(&inst->dst);
}

static void sed_emit_dst_src(Inst* inst) {
  sed_emit_dst(inst);
  emit_line("s/$/ /");
  sed_emit_src(inst);
}

static void sed_emit_set_dst(Inst* inst) {
  emit_line("G");
  emit_line("s/^\\([^\\n]*\\)\\n\\([^%c]* %c=\\)[^ ]*/\\2\\1/",
            SED_REG_NAMES[inst->dst.reg], SED_REG_NAMES[inst->dst.reg]);
  emit_line("x");
  emit_line("s/.*//");
}

static void sed_emit_add(Inst* inst) {
  static int id = 0;
  sed_emit_dst_src(inst);
  emit_line(" s/\\(.*\\) \\([0-9a-f]*\\)"
            "/\\1@ \\2@ fedcba9876543210 fedcba9876543210;/");
  emit_line(":add_loop_%d", id);
  emit_line(" s/\\(.\\)@\\([^@]*\\)\\(.\\)@\\(.\\)\\? "
            "\\([^ ]*\\1\\([^ ]*\\)\\) [^;]*\\(\\3[^;]*\\);"
            "/@\\2; \\4\\6\\7\\5 \\5 \\5;/");
  emit_line(" s/; .\\{16\\}\\(.\\)\\([0-9a-f]\\{15\\}\\([0-9a-f]\\)\\)\\?"
            "[0-9a-f]* \\([^;]*\\);\\(.*\\)/@\\3 \\4;\\1\\5/");

  emit_line(" /^@ @/!{");
  emit_line("  s/^@/0@/");
  emit_line("  s/ @/ 0@/");
  emit_line("  badd_loop_%d", id);
  emit_line(" }");

  emit_line(" s/@ @. .*;/;1/");
  emit_line(" s/.*;//");
  emit_line(" s/.*\\(......\\)$/\\1/");
  emit_line(" s/^0*\\([0-9a-f]\\)/\\1/");

  sed_emit_set_dst(inst);

  id++;
}

static void sed_emit_sub(Inst* inst) {
  static int id = 0;
  sed_emit_dst_src(inst);
  emit_line("s/^/1000000/");
  emit_line(" s/\\(.*\\) \\([0-9a-f]*\\)"
            "/\\1@ \\2@x fedcba9876543210 0123456789abcdef;@\\1 \\2/");
  emit_line(":sub_loop_%d", id);
  emit_line(" s/\\(.\\)@\\([^@]*\\)\\(.\\)@\\(.\\)\\? "
            "\\([^ ]*\\1\\([^ ]*\\)\\) \\([^;]*\\3\\([^;]*\\)\\);"
            "/@\\2; \\4\\8\\1\\6\\5 \\5 \\7;/");
  emit_line(" s/; .\\{16\\}\\(.\\)\\([0-9a-f]\\{15\\}\\([^ ]\\)\\)\\?"
            "[0-9a-f]* \\(.*\\);\\(.*\\)/@\\3 \\4;\\1\\5/");

  emit_line(" /^@ @/!{");
  emit_line("  s/^@/0@/");
  emit_line("  s/ @/ 0@/");
  emit_line("  bsub_loop_%d", id);
  emit_line(" }");

  emit_line(" s/@ @. .*;/;/");
  emit_line(" s/@.*//");
  emit_line(" s/.*\\(......\\)$/\\1/");
  emit_line(" s/^0*\\([0-9a-f]\\)/\\1/");

  sed_emit_set_dst(inst);

  id++;
}

static void sed_emit_cmp(Inst* inst) {
  static int id = 0;
  uint op = normalize_cond(inst->op, false);
  sed_emit_dst_src(inst);
  if (op == JLT || op == JLE) {
    emit_line("s/\\(.*\\) \\(.*\\)/\\2 \\1/");
    op = op == JLT ? JGT : JGE;
  }

  switch (op) {
  case JEQ:
    emit_line("s/^\\(.*\\) \\1$/1/");
    emit_line("s/^.* .*$/0/");
    break;

  case JNE:
    emit_line("s/^\\(.*\\) \\1$/0/");
    emit_line("s/^.* .*$/1/");
    break;

  case JGE:
  case JGT:
    emit_line("s/^..* $/1/");
    emit_line("s/^...* .$/1/");
    emit_line("s/^....* ..$/1/");
    emit_line("s/^.....* ...$/1/");
    emit_line("s/^......* ....$/1/");
    emit_line("s/^...... .....$/1/");
    emit_line("s/^ ..*$/0/");
    emit_line("s/^. ...*$/0/");
    emit_line("s/^.. ....*$/0/");
    emit_line("s/^... .....*$/0/");
    emit_line("s/^.... ......*$/0/");
    emit_line("s/^..... ......$/0/");
    emit_line(":cmp_loop_%d", id);
    emit_line("/^\\(.\\).* \\1/{");
    emit_line(" s/^\\(.\\)\\(.*\\) \\1/\\2 /");
    emit_line(" bcmp_loop_%d", id);
    emit_line("}");
    emit_line("s/$/;fedcba9876543210/");
    emit_line("s/^\\(.\\).* \\(.\\).*;.*\\1.*\\2.*/1/");
    if (op == JGE)
      emit_line("s/^\\(.*\\) \\1;.*/1/");
    emit_line("s/^.* .*/0/");
    emit_line("s/;.*//");
    id++;
    break;
  }
}

static void sed_emit_jmp(Inst* inst) {
  if (inst->jmp.type == REG) {
    sed_emit_value(&inst->jmp);
    emit_line("bjmp_reg");
  } else {
    emit_line("bpc_%x", inst->jmp.imm);
  }
}

static void sed_emit_inst(Inst* inst) {
  switch (inst->op) {
  case MOV:
    sed_emit_src(inst);
    sed_emit_set_dst(inst);
    break;

  case ADD:
    sed_emit_add(inst);
    break;

  case SUB:
    sed_emit_sub(inst);
    break;

  case LOAD:
    sed_emit_src(inst);
    emit_line("G");
    emit_line("s/^\\([^\\n]*\\)\\n.*m\\1=\\([^ ]*\\).*/@\\2/");
    emit_line("/^@/!s/.*/0/");
    emit_line("s/^@//");
    sed_emit_set_dst(inst);
    break;

  case STORE:
    sed_emit_dst_src(inst);
    emit_line("G");
    emit_line("/ \\([^\\n]*\\)\\n.*m\\1=/"
              "s/^\\([^ ]*\\) \\([^\\n]*\\)\\n\\(.*m\\2=\\)[^ ]*/@\\3\\1/");
    emit_line("/^@/!"
              "s/^\\([^ ]*\\) \\([^\\n]*\\)\\n\\(.*\\)/\\3m\\2=\\1 /");
    emit_line("s/^@//");
    emit_line("x");
    emit_line("s/.*//");
    break;

  case PUTC:
    sed_emit_src(inst);
    emit_line("s/^/0/");
    emit_line("s/.*\\(..\\)$/\\1/");
    emit_line("G");
    emit_line("s/^\\(..\\)\\n\\(.*o=[^ ]*\\)/\\2\\1/");
    emit_line("x");
    emit_line("s/.*//");
    break;

  case GETC: {
    emit_line("g");
    emit_line("/i= /s/.*/0/");
    emit_line("/i=[^ ]/{");
    emit_line("s/.*i=\\([^,]*\\),.*/\\1/");
    emit_line("x");
    emit_line("s/i=[^,]*,/i=/");
    emit_line("x");
    emit_line("}");
    sed_emit_set_dst(inst);
    break;
  }

  case EXIT:
    emit_line("bexit");
    break;

  case DUMP:
    //emit_line("x");
    //emit_line("p");
    //emit_line("x");
    break;

  case EQ:
  case NE:
  case LT:
  case GT:
  case LE:
  case GE:
    sed_emit_cmp(inst);
    sed_emit_set_dst(inst);
    break;

  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
    sed_emit_cmp(inst);
    emit_line("/^1$/{");
    emit_line(" s/.//");
    sed_emit_jmp(inst);
    emit_line("}");
    emit_line("s/.//");
    break;

  case JMP:
    sed_emit_jmp(inst);
    break;

  default:
    error("oops");
  }
}

static void sed_emit_jmp_table_rec(int depth, char* reg, int pc, int last_pc) {
  static const int WIDTH[] = {
    4096, 256, 16, 1
  };
  const int w = WIDTH[depth];
  for (int i = 0; i < 16 && pc <= last_pc; i++, pc += w) {
    reg[depth] = i > 9 ? 'a' + i - 10 : '0' + i;
    reg[depth+1] = 0;
    if (depth == 3) {
      emit_line("/^%s$/bpc_%x", reg, pc);
    } else {
      emit_line("/^%s/{", reg);
      sed_emit_jmp_table_rec(depth + 1, reg, pc, last_pc);
      emit_line("}");
    }
  }
}

static void sed_emit_jmp_table(int last_pc) {
  emit_line("");
  emit_line(":jmp_reg");
  emit_line("s/^/000/");
  emit_line("s/.*\\(....\\)/\\1/");
  char reg[5];
  sed_emit_jmp_table_rec(0, reg, 0, last_pc);
}

void target_sed(Module* module) {
  sed_init_state(module->data);

  int prev_pc = -1;
  for (Inst* inst = module->text; inst; inst = inst->next) {
    if (prev_pc != inst->pc) {
      emit_line("");
      emit_line(":pc_%x", inst->pc);
      emit_line("s/.*//");
    }
    prev_pc = inst->pc;
    sed_emit_inst(inst);
  }

  sed_emit_jmp_table(prev_pc);

  emit_line("");
  emit_line(":exit");
  emit_line("s/.*//");
  emit_line("x");
  emit_line("s/.*o=\\([^ ]*\\).*/\\1/");
  emit_line(":out_loop");
  emit_line("/^$/bout_done");
  for (int i = 0; i < 256; i++) {
    printf("/^%x%x/{s/..//\nx\n", i / 16, i % 16);
    if (i == 10) {
      emit_line("p\ns/.*//\nx\n}");
    } else {
      char buf[6];
      buf[0] = i;
      buf[1] = 0;
      if (i == '/' || i == '\\' || i == '&') {
        buf[0] = '\\';
        buf[1] = i;
        buf[2] = 0;
      } else if (i == 0 || (i >= 0xc2 && i <= 0xfd)) {
        sprintf(buf, "\\x%x", i);
      }
      emit_line("s/$/%s/\nx\n}", buf);
    }
  }
  emit_line("bout_loop");
  emit_line(":out_done");
  emit_line("x");
  emit_line("p");

  emit_line("}");
}
