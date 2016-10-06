#include <stdio.h>
#include <stdlib.h>

int g_buf = -1;

int getChar(void) {
  int r;
  if (g_buf != -1) {
    r = g_buf;
    g_buf = -1;
  } else {
    r = getchar();
    if (r == -1)
      exit(0);
  }
  return r;
}

int peekChar(void) {
  if (g_buf != -1)
    return g_buf;
  int c = getchar();
  g_buf = c;
  return c;
}

void ungetChar(int c) {
  g_buf = c;
}

const int g_close_char = ')';

#define ALLOC(s) calloc(s, 4)

#define ERROR(s) (puts(s), printExpr(a), putchar('\n'), exit(1))

typedef enum {
  NUM,
  STR,
  LIST,
  LAMBDA
} Type;

typedef struct List {
  struct Atom* head;
  struct List* tail;
} List;

typedef struct Atom {
  Type type;
  union {
    int num;
    int* str;
    List* list;
  };
} Atom;

typedef struct Table {
  int* key;
  Atom* value;
  struct Table* next;
} Table;

void printExpr(Atom* expr) {
  if (!expr) {
    fputs("nil", stdout);
    return;
  }

  if (expr->type == NUM) {
    unsigned short v = expr->num;
    if (v > 32767) {
      putchar('-');
      v = -v;
    }
    printf("%d", v);
    return;
  }

  if (expr->type == STR) {
    fputs((char*)expr->str, stdout);
    return;
  }

  putchar('(');
  if (expr->type == LAMBDA) {
    fputs("lambda ", stdout);
  }

  List* l = expr->list;
  while (l) {
    printExpr(l->head);
    l = l->tail;
    if (l)
      putchar(' ');
  }
  putchar(')');
}

List* cons(Atom* h, List* t) {
  List* s = (List*)ALLOC(sizeof(List));
  s->head = h;
  s->tail = t;
  return s;
}

Atom* createAtom(Type type) {
  Atom* a = (Atom*)ALLOC(sizeof(Atom));
  a->type = type;
  return a;
}

Atom* createInt(int n) {
  Atom* a = createAtom(NUM);
  a->num = n;
  return a;
}

Atom* createStr(int* s) {
  Atom* a = createAtom(STR);
  a->str = s;
  return a;
}

Atom* createList(List* l) {
  if (l == NULL)
    return NULL;
  Atom* a = createAtom(LIST);
  a->list = l;
  return a;
}

Atom* createLambda(List* l) {
  Atom* a = createAtom(LAMBDA);
  a->list = l;
  return a;
}

int atom(Atom* a) {
  return a == NULL || a->type != LIST || a->list == NULL;
}

int isList(Atom* a) {
  return a == NULL || a->type == LIST;
}

Atom* g_t;

Table* g_val;

Atom* parse(void);

void skipWS(void) {
  int c = getChar();
  while (c == ' ' || c == '\n')
    c = getChar();
  ungetChar(c);
}

Atom* parseList(void) {
  List* l = NULL;
  List* n = NULL;
  while (1) {
    skipWS();
    if (peekChar() == g_close_char) {
      getChar();
      break;
    }
    Atom* a = parse();
    List* t = cons(a, NULL);
    if (n) {
      n->tail = t;
    } else {
      l = n = t;
    }
    n = t;
  }
  return createList(l);
}

Atom* parseStr(int c) {
  int buf[99];
  int n = 0;
  while (c != ' ' && c != '\n' && c != '(' && c != ')') {
    buf[n] = c;
    c = getChar();
    n++;
  }
  ungetChar(c);

  if (n == 3 && buf[0] == 'n' && buf[1] == 'i' && buf[2] == 'l')
    return NULL;

  int* str = calloc(n + 1, 4);
  int i;
  for (i = 0; i < n; i++) {
    str[i] = buf[i];
  }
  str[i] = '\0';
  return createStr(str);
}

Atom* parseInt(int c) {
  int n = 0;
  int m = 0;
  if (c == '-') {
    m = 1;
  } else {
    n += c - '0';
  }

  while (1) {
    c = getChar();
    if (c >= '0' && c <= '9') {
      n *= 10;
      n += c - '0';
    } else {
      ungetChar(c);
      break;
    }
  }

  if (m) {
    if (n == 0)
      return parseStr('-');
    n = -n;
  }
  return createInt(n);
}

int eqStr(int* l, int* r) {
  int i;
  for (i = 0; l[i] || r[i]; i++) {
    if (l[i] != r[i])
      return 0;
  }
  return 1;
}

Table* lookupTable(Table* t, int* k) {
  while (t) {
    if (eqStr(t->key, k))
      return t;
    t = t->next;
  }
  return NULL;
}

void addTable(Table** t, int* k, Atom* v) {
  Table* nt = lookupTable(*t, k);
  if (!nt) {
    nt = (Table*)ALLOC(sizeof(Table));
    nt->next = *t;
    *t = nt;
  }
  nt->key = k;
  nt->value = v;
}

int eq(Atom* l, Atom* r);

int eqList(List* l, List* r) {
  while (l && r) {
    if (!eq(l->head, r->head))
      return 0;

    l = l->tail;
    r = r->tail;
  }

  return l == NULL && r == NULL;
}

int eq(Atom* l, Atom* r) {
  if (l == r)
    return 1;

  if (l == NULL || r == NULL)
    return 0;

  if (l->type != r->type)
    return 0;

  if (l->type == NUM)
    return l->num == r->num;

  if (l->type == STR)
    return eqStr(l->str, r->str);

  return eqList(l->list, r->list);
}

int getListSize(List* l) {
  int n = 0;
  while (l) {
    l = l->tail;
    n++;
  }
  return n;
}

Atom* eval(Atom* a, Table* val) {
  if (atom(a)) {
    if (a && a->type == STR) {
      Table* t = lookupTable(val, a->str);
      if (t)
        return t->value;
      t = lookupTable(g_val, a->str);
      if (t)
        return t->value;
    }
    return a;
  }

  List* s = a->list;

  if (s->head->type == STR) {
    int* fn = s->head->str;
    if (fn[0] == 'i' && fn[1] == 'f' && fn[2] == '\0') {
      if (getListSize(s) != 4)
        ERROR("invalid if");

      Atom* c = eval(s->tail->head, val);
      if (c) {
        return eval(s->tail->tail->head, val);
      } else {
        return eval(s->tail->tail->tail->head, val);
      }
    } else if (fn[0] == 'q' && fn[1] == 'u' && fn[2] == 'o' &&
               fn[3] == 't' && fn[4] == 'e' && fn[5] == '\0') {
      if (getListSize(s) != 2)
        ERROR("invalid quote");

      return s->tail->head;
    } else if (fn[0] == 'd' && fn[1] == 'e' && fn[2] == 'f' &&
               fn[3] == 'i' && fn[4] == 'n' && fn[5] == 'e' &&
               fn[6] == '\0') {
      if (getListSize(s) != 3 || s->tail->head->type != STR)
        ERROR("invalid define");

      Atom* e = eval(s->tail->tail->head, val);
      addTable(&g_val, s->tail->head->str, e);
      return e;
    } else if (fn[0] == 'l' && fn[1] == 'a' && fn[2] == 'm' &&
               fn[3] == 'b' && fn[4] == 'd' && fn[5] == 'a' &&
               fn[6] == '\0') {
      if (getListSize(s) != 3 || !isList(s->tail->head))
        ERROR("invalid lambda");

      return createLambda(s->tail);
    } else if (fn[0] == 'd' && fn[1] == 'e' && fn[2] == 'f' &&
               fn[3] == 'u' && fn[4] == 'n' && fn[5] == '\0') {
      if (getListSize(s) != 4 ||
          s->tail->head->type != STR || !isList(s->tail->tail->head))
        ERROR("invalid defun");

      Atom* e = createLambda(s->tail->tail);
      addTable(&g_val, s->tail->head->str, e);
      return e;
    }
  }

  Atom* hd = eval(s->head, val);

  if (hd->type == LAMBDA) {
    List* args = hd->list->head ? hd->list->head->list : NULL;

    if (getListSize(s) - 1 != getListSize(args))
      ERROR("invalid lambda application");

    Table* nval = NULL;
    List* params = s->tail;
    while (args) {
      addTable(&nval, args->head->str, eval(params->head, val));
      args = args->tail;
      params = params->tail;
    }

    Atom* expr = hd->list->tail->head;
    return eval(expr, nval);
  }

  if (hd->type == STR) {
    int* fn = hd->str;
    int op = fn[0];
    if (((op == '+' || op == '-' || op == '*' || op == '/') &&
         fn[1] == '\0') ||
        (op == 'm' && fn[1] == 'o' && fn[2] == 'd' && fn[3] == '\0')) {
      if (getListSize(s) != 3)
        ERROR("invalid arith");
      Atom* l = eval(s->tail->head, val);
      Atom* r = eval(s->tail->tail->head, val);
      if (l->type != NUM || r->type != NUM)
        ERROR("invalid arith");
      int result = 0;
      if (op == '+') result = l->num + r->num;
      else if (op == '-') result = l->num - r->num;
      else if (op == '*') result = l->num * r->num;
      else if (op == '/') result = l->num / r->num;
      else result = l->num % r->num;
      return createInt(result);
    } else if (op == 'e' && fn[1] == 'q' && fn[2] == '\0') {
      if (getListSize(s) != 3)
        ERROR("invalid eq");

      Atom* l = eval(s->tail->head, val);
      Atom* r = eval(s->tail->tail->head, val);
      if (eq(l, r))
        return g_t;
      else
        return NULL;
    } else if (op == 'c' && (fn[1] == 'a' || fn[1] == 'd') &&
               fn[2] == 'r' && fn[3] == '\0') {
      Atom* e = eval(s->tail->head, val);

      if (e == NULL)
        return NULL;

      if (e->type != LIST || getListSize(s) != 2)
        ERROR("invalid car/cdr");

      if (fn[1] == 'a')
        return e->list->head;
      else
        return createList(e->list->tail);
    } else if (op == 'c' && fn[1] == 'o' && fn[2] == 'n' &&
               fn[3] == 's' && fn[4] == '\0') {
      if (getListSize(s) != 3)
        ERROR("invalid cons");

      Atom* l = eval(s->tail->head, val);
      Atom* r = eval(s->tail->tail->head, val);

      if (r && r->type != LIST)
        ERROR("invalid cons");

      return createList(cons(l, r ? r->list : NULL));
    } else if (op == 'a' && fn[1] == 't' && fn[2] == 'o' &&
               fn[3] == 'm' && fn[4] == '\0') {
      if (getListSize(s) != 2)
        ERROR("invalid atom");

      Atom* e = eval(s->tail->head, val);

      if (atom(e))
        return g_t;
      else
        return NULL;
    } else if (op == 'n' && fn[1] == 'e' && fn[2] == 'g' &&
               fn[3] == '?' && fn[4] == '\0') {
      if (getListSize(s) != 2)
        ERROR("invalid neg?");

      Atom* e = eval(s->tail->head, val);

      if (e->type == NUM && e->num < 0)
        return g_t;
      else
        return NULL;
    } else if (fn[0] == 'p' && fn[1] == 'r' && fn[2] == 'i' &&
               fn[3] == 'n' && fn[4] == 't' && fn[5] == '\0') {
      if (getListSize(s) != 2)
        ERROR("invalid print");

      Atom* e = eval(s->tail->head, val);
      printExpr(e);
      putchar('\n');
      return e;
    }

    fputs((char*)fn, stdout);
    putchar(':');
    putchar(' ');
    ERROR("undefined function");
  }

  ERROR("invalid function application");
  return NULL;
}

Atom* parse(void) {
  skipWS();
  int c = getChar();
  if (c == '(') {
    return parseList();
  } else if (c == '-' || (c >= '0' && c <= '9')) {
    return parseInt(c);
  } else if (c == ';') {
    while (c != '\n') {
      c = getChar();
    }
    return parse();
  } else {
    return parseStr(c);
  }
}

int main() {
  int buf[2];
  buf[0] = 't';
  buf[1] = '\0';
  g_t = createStr(buf);

  while (1) {
    putchar('>');
    putchar(' ');
    Atom* expr = parse();
    Atom* result = eval(expr, NULL);
    printExpr(result);
    putchar('\n');
  }
}
