#include <stdlib.h>
#include <ir/ir.h>
#include <target/util.h>

typedef enum {
  SQLITE3_A = 0,
  SQLITE3_B,
  SQLITE3_C,
  SQLITE3_D,
  SQLITE3_BP,
  SQLITE3_SP,
  SQLITE3_PC,
  SQLITE3_STEP,
  SQLITE3_RUN,
  SQLITE3_MEM,
  SQLITE3_IN,
  SQLITE3_OUT,
  SQLITE3_NUM_COLS
} SQLite3Col;

static const char* COL_NAMES[SQLITE3_NUM_COLS] = {
  "a", "b", "c", "d", "bp", "sp", "pc", "step",
  "running", "mem", "stdin", "stdout"
};

typedef struct SQLite3CaseExpr_ {
  int pc;
  int step;
  const char* expr;
  struct SQLite3CaseExpr_* next;
} SQLite3CaseExpr;


static SQLite3CaseExpr* sqlite3_add_expr(SQLite3CaseExpr* expr,
                                         int pc,
                                         int step,
                                         const char* expr_str) {
  SQLite3CaseExpr* e = malloc(sizeof(SQLite3CaseExpr));
  e->pc = pc;
  e->step = step;
  e->expr = expr_str;
  e->next = 0;
  expr->next = e;
  return e;
}

const char* sqlite3_cmp_str(Inst* inst) {
  int op = normalize_cond(inst->op, 0);
  const char* op_str;
  switch (op) {
    case JEQ: op_str = "="; break;
    case JNE: op_str = "<>"; break;
    case JLT: op_str = "<"; break;
    case JGT: op_str = ">"; break;
    case JLE: op_str = "<="; break;
    case JGE: op_str = ">="; break;
    default:
      error("oops");
  }
  return format("%s %s %s", COL_NAMES[inst->dst.reg], op_str, src_str(inst));
}

static void sqlite3_transpose_insts(Inst* inst, SQLite3CaseExpr* cols[]) {
  
  int step = 0;
  int prev_pc = -1;
  SQLite3Col col_idx = 0;
  const char* expr = NULL;
  int updcols = 0;

  for (; inst; inst = inst->next) {
    if (prev_pc != inst-> pc) {
      if (prev_pc != -1) {
        step++;
        cols[SQLITE3_PC] = sqlite3_add_expr(cols[SQLITE3_PC], prev_pc, step, "pc+1");
        cols[SQLITE3_STEP] = sqlite3_add_expr(cols[SQLITE3_STEP], prev_pc, step, "0");
      }
      updcols = 0;
      step = 0;
    }

    int usecol = ((inst->src.type == REG) ? (1 << inst->src.reg) : 0) |
                 ((inst->dst.type == REG) ? (1 << inst->dst.reg) : 0);

    if ((usecol & updcols) != 0) {
      step++;
      updcols = 0;
    }

    switch (inst->op) {
    case MOV:
      col_idx = (SQLite3Col)inst->dst.reg;
      expr = src_str(inst);
      break;

    case ADD:
      col_idx = (SQLite3Col)inst->dst.reg;
      expr = format("(%s + %s) & " UINT_MAX_STR,
                    COL_NAMES[inst->dst.reg], src_str(inst));
      break;

    case SUB:
      col_idx = (SQLite3Col)inst->dst.reg;
      expr = format("(%s - %s) & " UINT_MAX_STR,
                COL_NAMES[inst->dst.reg], src_str(inst));
      break;

    case LOAD:
      col_idx = (SQLite3Col)inst->dst.reg;
      expr = format("coalesce(json_extract(mem, '$.'||%s),0)", src_str(inst));
      break;

    case STORE:
      col_idx = SQLITE3_MEM;
      expr = format("json_set(mem, '$.'||%s, %s)",
                    src_str(inst), COL_NAMES[inst->dst.reg]);
      break;

    case PUTC:
      col_idx = SQLITE3_OUT;
      expr = format("stdout||char(%s)", src_str(inst));
      break;

    case GETC:
      cols[SQLITE3_IN] = sqlite3_add_expr(cols[SQLITE3_IN], inst->pc, step,
                                          "substr(stdin, 2)");
      col_idx = (SQLite3Col)inst->dst.reg;
      // todo: can't read a single byte from multibyte character
      expr = "CASE WHEN length(stdin) = 0 THEN 0 ELSE unicode(stdin) END";
      break;

    case EXIT:
      col_idx = SQLITE3_RUN;
      expr = "0";
      break;

    case DUMP:
      goto next_inst;

    case EQ:
    case NE:
    case LT:
    case GT:
    case LE:
    case GE:
      col_idx = (SQLite3Col)inst->dst.reg;
      expr = sqlite3_cmp_str(inst);
      break;

    case JEQ:
    case JNE:
    case JLT:
    case JGT:
    case JLE:
    case JGE:
      expr = format("CASE WHEN %s THEN 0 ELSE step+1 END", sqlite3_cmp_str(inst));
      cols[SQLITE3_STEP] = sqlite3_add_expr(cols[SQLITE3_STEP], inst->pc, step, expr);
      col_idx = SQLITE3_PC;
      expr = format("CASE WHEN %s THEN %s ELSE pc END",
                    sqlite3_cmp_str(inst), value_str(&inst->jmp));
      break;
    case JMP:
      cols[SQLITE3_STEP] = sqlite3_add_expr(cols[SQLITE3_STEP], inst->pc, step, "0");
      col_idx = SQLITE3_PC;
      expr = value_str(&inst->jmp);
      break;

    default:
      error("oops");
    }

    cols[col_idx] = sqlite3_add_expr(cols[col_idx], inst->pc, step, expr);

    if (inst->op == GETC || inst->op == PUTC || inst->op == STORE) {
      step++;
      updcols = 0;
    } else {
      updcols |= (1 << col_idx);
    }

  next_inst:
    prev_pc = inst->pc;
  }
  cols[SQLITE3_PC] = sqlite3_add_expr(cols[SQLITE3_PC], prev_pc, step, "pc+1");
  cols[SQLITE3_STEP] = sqlite3_add_expr(cols[SQLITE3_STEP], prev_pc, step, "0");
}


void sqlite3_emit_stdin() {
  emit_line("DROP TABLE IF EXISTS stdin;");
  emit_line("CREATE TABLE stdin(i BLOB);");
  emit_line("INSERT INTO stdin(i) VALUES(readfile('input.txt'));");
}

static void sqlite3_emit_column(SQLite3CaseExpr* ce, SQLite3Col col) {
  if (ce == NULL) {
    emit_line(COL_NAMES[col]);
    return;
  }

  int prev_part = -1;
  int prev_pc = -1;
  
  emit_line("CASE pc / %d", CHUNKED_FUNC_SIZE);
  for (; ce != NULL; ce = ce->next) {
    int part = ce->pc / CHUNKED_FUNC_SIZE;
    if (ce->pc != prev_pc) {
      if (prev_pc != -1) {
        // end step
        if (col == SQLITE3_STEP) {
          emit_line("ELSE step + 1 END");
        } else {
          emit_line("ELSE %s END", COL_NAMES[col]);
        }
        dec_indent();
      }
      if (part != prev_part) {
        if (prev_pc != -1) {
          // end pc
          if (col == SQLITE3_PC || col == SQLITE3_STEP) {
            emit_line("END");
          } else {
            emit_line("ELSE %s END", COL_NAMES[col]);
          }
          dec_indent();
        }
        emit_line("WHEN %d THEN", part);
        inc_indent();
        emit_line("CASE pc");
      }
      emit_line("WHEN %d THEN", ce->pc);
      inc_indent();
      emit_line("CASE step");
    }
    emit_line("WHEN %d THEN %s", ce->step, ce->expr);
    prev_part = part;
    prev_pc = ce->pc;
  }

  if (col == SQLITE3_STEP) {
    emit_line("ELSE step+1 END");
  } else {
    emit_line("ELSE %s END", COL_NAMES[col]);
  }
  for (int i = 0; i<2; i++) {
    dec_indent();
    if (col == SQLITE3_PC || col == SQLITE3_STEP) {
      emit_line("END");
    } else {
      emit_line("ELSE %s END", COL_NAMES[col]);
    }
  }
}


void target_sqlite3(Module* module) {
  reg_names = COL_NAMES;
  SQLite3CaseExpr roots[SQLITE3_NUM_COLS] = {};
  SQLite3CaseExpr* cols[SQLITE3_NUM_COLS];
  for (int i = 0; i < SQLITE3_NUM_COLS; i++) {
    roots[i].next = NULL;
    cols[i] = &roots[i];
  }
  sqlite3_transpose_insts(module->text, cols);
  for (int i = 0; i < SQLITE3_NUM_COLS; i++) {
    cols[i] = roots[i].next;
  }
  
  sqlite3_emit_stdin();

  emit_line("WITH");
  inc_indent();
  emit_line("elvm AS (");
  inc_indent();
  emit_line("SELECT");
  inc_indent();
  for (int i = 0; i < 7; i++) {
    emit_line("0 %s,", COL_NAMES[i]);
  }
  emit_line("0 step,");
  emit_line("1 running,");
  emit_line("json('{");
  int mp = 0;
  for (Data* data = module->data; data; data = data->next, mp++) {
    if (data->v) {
      emit_line(" %c\"%d\":%d", mp ? ',' : ' ', mp, data->v);
    }
  }
  emit_line("}') mem,");
  emit_line("(SELECT i FROM stdin) stdin,");
  emit_line("'' stdout,");
  emit_line("0 cycle");
  dec_indent();
  emit_line("UNION ALL SELECT");
  inc_indent();

  for (int i = 0; i < SQLITE3_NUM_COLS; i++) {
    sqlite3_emit_column(cols[i], i);
    emit_line(",");
  }
  emit_line("cycle + 1");
  dec_indent();
  emit_line("FROM elvm WHERE running = 1");
  dec_indent();
  emit_line(")");
  dec_indent();

  emit_line("SELECT writefile('output.txt', stdout) FROM elvm"
            " WHERE running = 0;");
}
