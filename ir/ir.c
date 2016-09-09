#include "ir.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "table.h"

typedef struct {
  const char* filename;
  int lineno;
  int col;
  FILE* fp;
  Table* symtab;
} Parser;

static void error(Parser* p, const char* msg) {
  fprintf(stderr, "%s:%d:%d: %s\n", p->filename, p->lineno, p->col, msg);
  exit(1);
}

static int ir_getc(Parser* p) {
  int c = fgetc(p->fp);
  if (c == '\n') {
    p->lineno++;
    p->col = 0;
  } else {
    p->col++;
  }
  return c;
}

static void ir_ungetc(Parser* p, int c) {
  if (c == '\n') {
    p->lineno--;
  }
  ungetc(c, p->fp);
}

static void skip_until_ret(Parser* p) {
  int c;
  for (;;) {
    c = ir_getc(p);
    if (c == '\n' || c == EOF)
      break;
  }
  ir_ungetc(p, c);
}

static void skip_ws(Parser* p) {
  int c;
  for (;;) {
    c = ir_getc(p);
    if (!isspace(c))
      break;
  }
  ir_ungetc(p, c);
}

static void read_while_ident(Parser* p, char* buf, int len) {
  while (len--) {
    int c = ir_getc(p);
    if (!isalnum(c) && c != '_') {
      ir_ungetc(p, c);
      *buf = 0;
      return;
    }
    *buf++ = c;
  }
  error(p, "too long ident");
}

static int read_int(Parser* p, int c) {
  bool is_minus = false;
  int r = 0;
  if (c == '-') {
    is_minus = true;
    c = ir_getc(p);
    if (!isdigit(c))
      error(p, "digit expected");
  }
  while ('0' <= c && c <= '9') {
    r *= 10;
    r += c - '0';
    c = ir_getc(p);
  }
  ir_ungetc(p, c);
  return is_minus ? -r : r;
}

static Data* add_data(Data* d, int* mp, int v) {
  Data* n = malloc(sizeof(Data));
  n->next = 0;
  n->v = v;
  d->next = n;
  ++*mp;
  return n;
}

static Module* parse_eir(Parser* p) {
  Inst text_root = {};
  Inst* text = &text_root;
  Data data_root = {};
  Data* data = &data_root;
  char buf[32];
  int in_text = 1;
  int pc = 0;
  int mp = 0;
  int c;
  bool prev_jmp = 1;
  p->lineno = 1;

  text->next = calloc(1, sizeof(Inst));
  text = text->next;
  text->op = JMP;
  text->pc = pc++;
  text->lineno = -1;
  text->jmp.type = TMP;
  text->jmp.tmp = "main";
  text->next = 0;
  p->symtab = TABLE_ADD(p->symtab, "main", 1);

  for (;;) {
    skip_ws(p);
    c = ir_getc(p);
    if (c == EOF)
      break;

    if (c == '#') {
      skip_until_ret(p);
    } else if (c == '_' || c == '.' || isalpha(c)) {
      buf[0] = c;
      read_while_ident(p, buf + 1, 30);
      Op op = OP_UNSET;
      if (!strcmp(buf, "mov")) {
        op = MOV;
      } else if (!strcmp(buf, "add")) {
        op = ADD;
      } else if (!strcmp(buf, "sub")) {
        op = SUB;
      } else if (!strcmp(buf, "load")) {
        op = LOAD;
      } else if (!strcmp(buf, "store")) {
        op = STORE;
      } else if (!strcmp(buf, "putc")) {
        op = PUTC;
      } else if (!strcmp(buf, "getc")) {
        op = GETC;
      } else if (!strcmp(buf, "exit")) {
        op = EXIT;
      } else if (!strcmp(buf, "dump")) {
        op = DUMP;
      } else if (!strcmp(buf, "jeq")) {
        op = JEQ;
      } else if (!strcmp(buf, "jne")) {
        op = JNE;
      } else if (!strcmp(buf, "jlt")) {
        op = JLT;
      } else if (!strcmp(buf, "jgt")) {
        op = JGT;
      } else if (!strcmp(buf, "jle")) {
        op = JLE;
      } else if (!strcmp(buf, "jge")) {
        op = JGE;
      } else if (!strcmp(buf, "jmp")) {
        op = JMP;
      } else if (!strcmp(buf, "eq")) {
        op = EQ;
      } else if (!strcmp(buf, "ne")) {
        op = NE;
      } else if (!strcmp(buf, "lt")) {
        op = LT;
      } else if (!strcmp(buf, "gt")) {
        op = GT;
      } else if (!strcmp(buf, "le")) {
        op = LE;
      } else if (!strcmp(buf, "ge")) {
        op = GE;
      } else if (!strcmp(buf, ".text")) {
        in_text = 1;
        continue;
      } else if (!strcmp(buf, ".data")) {
        in_text = 0;
        continue;
      } else if (!strcmp(buf, ".long")) {
        if (in_text)
          error(p, "in text");
        skip_ws(p);

        c = ir_getc(p);
        if (!isdigit(c) && c != '-')
          error(p, "number expected");
        data = add_data(data, &mp, read_int(p, c));
        continue;
      } else if (!strcmp(buf, ".string")) {
        if (in_text)
          error(p, "in text");
        skip_ws(p);
        if (ir_getc(p) != '"')
          error(p, "expected open '\"'");

        c = ir_getc(p);
        while (c != '"') {
          if (c == '\\') {
            c = ir_getc(p);
            if (c == 'n')
              c = 10;
            else
              error(p, "unknown escape");
            data = add_data(data, &mp, c);
          } else {
            data = add_data(data, &mp, c);
          }
          c = ir_getc(p);
        }
        data = add_data(data, &mp, 0);

        continue;
      } else {
        c = ir_getc(p);
        if (c == ':') {
          int value = 0;
          if (in_text) {
            if (!prev_jmp)
              pc++;
            value = pc;
          } else {
            value = mp;
          }
          p->symtab = TABLE_ADD(p->symtab, buf, value);
          continue;
        }
        error(p, "unknown op");
      }

      if (op < 0) {
        error(p, "oops");
      }
      int argc;
      if (op <= STORE)
        argc = 2;
      else if (op <= GETC)
        argc = 1;
      else if (op <= EXIT)
        argc = 0;
      else if (op <= JGE)
        argc = 3;
      else if (op <= JMP)
        argc = 1;
      else if (op <= GE)
        argc = 2;
      else if (op == DUMP)
        argc = 0;
      else
        error(p, "oops");

      text->next = calloc(1, sizeof(Inst));
      text = text->next;
      text->op = op;
      text->pc = pc;
      text->lineno = p->lineno;

      Value args[3];
      for (int i = 0; i < argc; i++) {
        skip_ws(p);
        if (i) {
          c = ir_getc(p);
          if (c != ',')
            error(p, "comma expected");
          skip_ws(p);
        }

        Value a;
        c = ir_getc(p);
        if (isdigit(c) || c == '-') {
          a.type = IMM;
          a.imm = read_int(p, c);
        } else {
          buf[0] = c;
          read_while_ident(p, buf + 1, 30);
          if (isupper(c)) {
            a.type = REG;
            if (!strcmp(buf, "A")) {
              a.reg = A;
            } else if (!strcmp(buf, "B")) {
              a.reg = B;
            } else if (!strcmp(buf, "C")) {
              a.reg = C;
            } else if (!strcmp(buf, "D")) {
              a.reg = D;
            } else if (!strcmp(buf, "SP")) {
              a.reg = SP;
            } else if (!strcmp(buf, "BP")) {
              a.reg = BP;
            } else {
              error(p, "unknown reg");
            }
          } else {
            a.type = TMP;
            a.tmp = strdup(buf);
          }
        }
        args[i] = a;
      }

      prev_jmp = false;
      switch (op) {
        case MOV:
        case ADD:
        case SUB:
        case LOAD:
        case STORE:
        case EQ:
        case NE:
        case LT:
        case GT:
        case LE:
        case GE:
          text->src = args[1];
        case GETC:
          text->dst = args[0];
          break;
        case PUTC:
          text->src = args[0];
        case EXIT:
        case DUMP:
          break;
        case JEQ:
        case JNE:
        case JLT:
        case JGT:
        case JLE:
        case JGE:
          text->dst = args[1];
          text->src = args[2];
        case JMP:
          text->jmp = args[0];
          pc++;
          prev_jmp = true;
          break;
        default:
          error(p, "oops");
      }

      //dump_inst(text);
      //fprintf(stderr, "\n");
    } else {
      error(p, "unexpected char");
    }
  }

  p->symtab = TABLE_ADD(p->symtab, "_edata", mp);

  Module* m = malloc(sizeof(Module));
  m->text = text_root.next;
  m->data = data_root.next;
  return m;
}

static void resolve(Value* v, Table* symtab) {
  if (v->type != TMP)
    return;
  const char* name = (const char*)v->tmp;
  if (!TABLE_GET(symtab, name, &v->imm)) {
    fprintf(stderr, "undefined sym: %s\n", name);
    exit(1);
  }
  //fprintf(stderr, "resolved: %s %d\n", name, v->imm);
  v->type = IMM;
}

static void resolve_syms(Module* mod, Table* symtab) {
  for (Inst* inst = mod->text; inst; inst = inst->next) {
    resolve(&inst->dst, symtab);
    resolve(&inst->src, symtab);
    resolve(&inst->jmp, symtab);
  }
}

Module* load_eir_impl(const char* filename, FILE* fp) {
  Parser parser = {
    .filename = filename,
    .fp = fp
  };
  Module* mod = parse_eir(&parser);
  resolve_syms(mod, parser.symtab);
  return mod;
}

Module* load_eir(FILE* fp) {
  return load_eir_impl("<stdin>", fp);
}

Module* load_eir_from_file(const char* filename) {
  FILE* fp = fopen(filename, "r");
  Module* r = load_eir_impl(filename, fp);
  fclose(fp);
  return r;
}

void dump_op(Op op) {
  static const char* op_strs[] = {
    "mov", "add", "sub", "load", "store", "putc", "getc", "exit",
    "jeq", "jne", "jlt", "jgt", "jle", "jge", "jmp", "xxx",
    "eq", "ne", "lt", "gt", "le", "ge", "dump"
  };
  fprintf(stderr, "%s", op_strs[op]);
}

void dump_val(Value val) {
  static const char* reg_strs[] = {
    "A", "B", "C", "D", "BP", "SP"
  };
  if (val.type == REG) {
    fprintf(stderr, "%s", reg_strs[val.reg]);
  } else {
    fprintf(stderr, "%d", val.imm);
  }
}

void dump_inst(Inst* inst) {
  dump_op(inst->op);
  switch (inst->op) {
    case MOV:
    case ADD:
    case SUB:
    case LOAD:
    case STORE:
    case EQ:
    case NE:
    case LT:
    case GT:
    case LE:
    case GE:
      fprintf(stderr, " ");
      dump_val(inst->dst);
      fprintf(stderr, " ");
      dump_val(inst->src);
      break;
    case PUTC:
    case GETC:
      fprintf(stderr, " ");
      dump_val(inst->dst);
    case EXIT:
    case DUMP:
      break;
    case JEQ:
    case JNE:
    case JLT:
    case JGT:
    case JLE:
    case JGE:
      fprintf(stderr, " ");
      dump_val(inst->dst);
      fprintf(stderr, " ");
      dump_val(inst->src);
    case JMP:
      fprintf(stderr, " ");
      dump_val(inst->jmp);
      break;
    default:
      fprintf(stderr, "oops\n");
      exit(1);
  }
  fprintf(stderr, " pc=%d @%d\n", inst->pc, inst->lineno);
}

#ifdef TEST

int main(int argc, char* argv[]) {
  if (argc < 2) {
    fprintf(stderr, "no input file\n");
    exit(1);
  }
  Module* m = load_eir_from_file(argv[1]);
  for (Inst* inst = m->text; inst; inst = inst->next) {
    dump_inst(inst);
  }
  return 0;
}

#endif
