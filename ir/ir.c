#include "ir.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "table.h"

static const char* g_filename = "<stdin>";
static int g_lineno;
static int g_col;

static void error(const char* msg) {
  fprintf(stderr, "%s:%d:%d: %s\n", g_filename, g_lineno, g_col, msg);
  exit(1);
}

static int my_getc(FILE* fp) {
  int c = fgetc(fp);
  if (c == '\n') {
    g_lineno++;
    g_col = 0;
  } else {
    g_col++;
  }
  return c;
}

static void my_ungetc(int c, FILE* fp) {
  if (c == '\n') {
    g_lineno--;
  }
  ungetc(c, fp);
}

static void skip_until_ret(FILE* fp, int is_comment) {
  int c;
  for (;;) {
    c = my_getc(fp);
    if (c == '\n' || c == EOF)
      break;
    if (c == '#') {
      is_comment = 1;
      continue;
    }
    if (!is_comment && !isspace(c)) {
      error("unexpected char");
    }
  }
  my_ungetc(c, fp);
}

static void skip_ws(FILE* fp) {
  int c;
  for (;;) {
    c = my_getc(fp);
    if (!isspace(c))
      break;
  }
  my_ungetc(c, fp);
}

static void read_while_ident(FILE* fp, char* p, int len) {
  while (len--) {
    int c = my_getc(fp);
    if (!isalnum(c) && c != '_') {
      my_ungetc(c, fp);
      *p = 0;
      return;
    }
    *p++ = c;
  }
  error("too long ident");
}

static int read_int(FILE* fp, int c) {
  bool is_minus = false;
  int r = 0;
  if (c == '-') {
    is_minus = true;
    c = my_getc(fp);
    if (!isdigit(c))
      error("digit expected");
  }
  while ('0' <= c && c <= '9') {
    r *= 10;
    r += c - '0';
    c = my_getc(fp);
  }
  my_ungetc(c, fp);
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

static Module* parse_eir(FILE* fp, Table** symtab) {
  Inst text_root;
  Inst* text = &text_root;
  Data data_root;
  Data* data = &data_root;
  char buf[32];
  int in_text = 1;
  int pc = 0;
  int mp = 0;
  int c;
  g_lineno = 1;

  for (;;) {
    skip_ws(fp);
    c = my_getc(fp);
    if (c == EOF)
      break;

    if (c == '#') {
      skip_until_ret(fp, 1);
    } else if (c == '_' || c == '.' || isalpha(c)) {
      buf[0] = c;
      read_while_ident(fp, buf + 1, 30);
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
          error("in text");
        skip_ws(fp);

        c = my_getc(fp);
        if (!isdigit(c) && c != '-')
          error("number expected");
        data = add_data(data, &mp, read_int(fp, c));
        continue;
      } else if (!strcmp(buf, ".string")) {
        if (in_text)
          error("in text");
        skip_ws(fp);
        if (my_getc(fp) != '"')
          error("expected open '\"'");

        c = my_getc(fp);
        while (c != '"') {
          if (c == '\\') {
            c = my_getc(fp);
            if (c == 'n')
              c = 10;
            else
              error("unknown escape");
            data = add_data(data, &mp, c);
          } else {
            data = add_data(data, &mp, c);
          }
          c = my_getc(fp);
        }
        data = add_data(data, &mp, 0);

        continue;
      } else {
        c = my_getc(fp);
        if (c == ':') {
          int value = in_text ? ++pc : mp;
          *symtab = TABLE_ADD(*symtab, buf, value);
          continue;
        }
        error("unknown op");
      }

      if (op < 0) {
        error("oops");
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
        error("oops");

      text->next = calloc(1, sizeof(Inst));
      text = text->next;
      text->op = op;
      text->pc = pc;

      Value args[3];
      for (int i = 0; i < argc; i++) {
        skip_ws(fp);
        if (i) {
          c = my_getc(fp);
          if (c != ',')
            error("comma expected");
          skip_ws(fp);
        }

        Value a;
        c = my_getc(fp);
        if (isdigit(c) || c == '-') {
          a.type = IMM;
          a.imm = read_int(fp, c);
        } else {
          buf[0] = c;
          read_while_ident(fp, buf + 1, 30);
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
              error("unknown reg");
            }
          } else {
            a.type = TMP;
            a.tmp = strdup(buf);
          }
        }
        args[i] = a;
      }

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
          break;
        default:
          error("oops");
      }

      //dump_inst(text);
      //fprintf(stderr, "\n");
    } else {
      error("unexpected char");
    }
  }

  *symtab = TABLE_ADD(*symtab, "_edata", mp);

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

Module* load_eir(FILE* fp) {
  Table* symtab = 0;
  Module* mod = parse_eir(fp, &symtab);
  resolve_syms(mod, symtab);
  return mod;
}

Module* load_eir_from_file(const char* filename) {
  g_filename = filename;
  FILE* fp = fopen(filename, "r");
  Module* r = load_eir(fp);
  fclose(fp);
  return r;
}

void dump_op(Op op) {
  static const char* op_strs[] = {
    "mov", "add", "sub", "load", "store", "putc", "getc", "exit",
    "jeq", "jne", "jlt", "jgt", "jle", "jge", "jmp", "xxx",
    "eq", "ne", "lt", "gt", "le", "ge"
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
      break;
    case JEQ:
    case JNE:
    case JLT:
    case JGT:
    case JLE:
    case JGE:
    case JMP:
      fprintf(stderr, " ");
      dump_val(inst->jmp);
      fprintf(stderr, " ");
      dump_val(inst->dst);
      fprintf(stderr, " ");
      dump_val(inst->src);
      break;
    default:
      error("oops");
  }
  fprintf(stderr, "\n");
}

#ifdef TEST

int main(int argc, char* argv[]) {
  if (argc < 2)
    error("no input file");
  Module* m = load_eir_from_file(argv[1]);
  for (Inst* inst = m->text; inst; inst = inst->next) {
    dump_inst(inst);
  }
  return 0;
}

#endif
