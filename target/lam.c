#include <ir/ir.h>
#include <target/util.h>


static char LAM_VM[] =
"(\\x.\\y.\\z.\\a.\\b.((\\c.((\\d.((\\e.((\\f.((\\g.((\\h.(a ((\\i.(i (d ( \\j."
"\\k.(k (\\l.\\m.\\n.\\o.(o k (j m))) k)) a) (\\j.(i z (d (\\k.\\l.\\m.\\n.\\o."
"\\p.((\\q.((\\r.((\\s.(n (\\t.\\u.\\v.\\w.v) (\\t.t) (\\t.\\u.\\v.u) (\\t.\\u."
"u) (o (\\t.\\u.\\v.(o (k l m) p)) o) (n (\\t.\\u.((\\v.(t (\\w.\\A.\\B.((\\C.("
"A (C B) (s B C))) (\\C.\\D.(w (D ((\\E.(m (\\F.\\G.\\H.(E (y (\\I.\\J.(J (\\K."
"\\L.K) I)) F) G)) (E c m))) (\\E.\\F.(r B E (k l F u o)))) (\\E.(E (y (\\F.(F("
"\\G.\\H.H))) C) (v p))) A) (D (\\E.\\F.\\G.\\H.((\\I.(F (I G) (s G I))) (s H ("
"\\I.\\J.(E (e I C) (q J) (v p))))))) (D (\\E.\\F.((\\G.(f (\\H.\\I.I) (E (s F "
"e C)) G G (\\H.(r F)))) c)) v) (q C) (h l C (r D) v) (s D (g l C) k m u o p) ("
"D (\\E.\\F.(s E (f F F) C (\\G.(r E)))) v) (r D C v))))))) (k l m u o)))))) (h"
" p))) (g p))) (\\q.(h j q (\\r.(r (k l m) p))))))))))) (\\i.\\j.(d (\\k.\\l.\\"
"m.\\n.(l (\\o.\\p.\\q.(m (\\r.\\s.\\t.(k l s (\\u.\\v.(k v s (\\w.(n (\\A.(A u"
" w)))))))) (l n))) (n l l))) i c (\\k.\\l.(j k)))) b) (\\i.\\j.j))) (d (\\h.\\"
"i.\\j.\\k.(i (\\l.\\m.\\n.(j (\\o.\\p.\\q.(o (h l) (h m) p k)) (k i))) (k c)))"
"))) (d (\\g.\\h.\\i.\\j.\\k.(i (\\l.\\m.\\n.((\\o.(h (\\p.\\q.\\r.(l (h o) (o "
"q p))) (o (\\p.\\q.q) (\\p.\\q.q)))) (\\o.(g o m j (\\p.\\q.(l (k (\\r.(r p q)"
")) (k (\\r.(r q p))))))))) (k j)))))) (d (\\f.\\g.\\h.\\i.\\j.\\k.(i (\\l.\\m."
"\\n.(j (\\o.\\p.(f g h m p (\\q.\\r.((\\s.((\\t.((\\u.((\\v.(t s q (v (\\w.\\A"
".w)) (v (\\w.\\A.A)))) (t q (q (\\v.\\w.w) (\\v.\\w.v)) (u (\\v.\\w.v)) (u (\\"
"v.\\w.w))))) (\\u.\\v.(k v (\\w.(w u r)))))) (\\t.\\u.(l (s t u) (s u t))))) ("
"h o (o (\\s.\\t.t) (\\s.\\t.s))))))))) (k g i)))))) (d (\\e.\\f.\\g.(f (\\h.\\"
"i.\\j.(g (\\k.\\l.((\\m.(h (k m (\\n.\\o.\\p.o)) (k (\\n.\\o.\\p.p) m))) (e i "
"l))))) (\\h.\\i.\\j.h)))))) (\\d.((\\e.(d (e e))) (\\e.(d (e e))))))) ((\\c.(y"
" c (x c (\\d.\\e.e)))) (\\c.\\d.(d (\\e.\\f.e) c)))))";


static const int LAM_N_BITS = 24;
static const char LAM_8[] = "(\\x.\\y.(((\\x.\\y.(x(x y)))"
                            "(((\\y.(y y))(\\x.\\y.(x(x y))))x))y))";
static const char LAM_16[] = "((\\x.(x x x))(\\x.\\y.(x(x y))))";

static const char LAM_T[] = "(\\x.\\y.x)";
static const char LAM_NIL[] = "(\\x.\\y.y)";

static const char LAM_CONS_HEADER[] = "(\\f.(f";
static const char LAM_CONS_FOOTER[] = "))";

static const char LAM_INT_HEADER[] = "((\\x.\\y.";
static const char LAM_INT_BIT0[] = "x";
static const char LAM_INT_BIT1[] = "y";
static const char LAM_INT_FOOTER[] = ")(\\x.\\y.(y(\\x.\\a.x)x))"
                                     "(\\x.\\y.(y(\\x.\\a.a)x)))";

static const char LAM_REG_A[]  = "(\\x.(x(\\y.\\z.y)(\\y.\\z.z)))";
static const char LAM_REG_B[]  = "(\\x.(x(\\y.\\z.z)(\\x.(x(\\z.\\a.z)(\\x.(x(\\a.\\b.a)(\\a.\\b.b)))))))";
static const char LAM_REG_SP[] = "(\\x.(x(\\y.\\z.z)(\\x.(x(\\z.\\a.z)(\\x.(x(\\a.\\b.b)(\\a.\\b.b)))))))";
static const char LAM_REG_D[]  = "(\\x.(x(\\y.\\z.z)(\\x.(x(\\z.\\a.a)(\\x.(x(\\a.\\b.a)(\\a.\\b.b)))))))";
static const char LAM_REG_BP[] = "(\\x.(x(\\y.\\z.z)(\\x.(x(\\z.\\a.a)(\\x.(x(\\a.\\b.b)(\\x.(x(\\b.\\c.b)(\\b.\\c.c)))))))))";
static const char LAM_REG_C[]  = "(\\x.(x(\\y.\\z.z)(\\x.(x(\\z.\\a.a)(\\x.(x(\\a.\\b.b)(\\x.(x(\\b.\\c.c)(\\b.\\c.c)))))))))";
static const char LAM_INST_IO[]     = "(\\x.\\y.\\z.\\a.\\b.\\c.\\d.\\e.x)";
static const char LAM_INST_JMPCMP[] = "(\\x.\\y.\\z.\\a.\\b.\\c.\\d.\\e.y)";
static const char LAM_INST_CMP[]    = "(\\x.\\y.\\z.\\a.\\b.\\c.\\d.\\e.z)";
static const char LAM_INST_JMP[]    = "(\\x.\\y.\\z.\\a.\\b.\\c.\\d.\\e.a)";
static const char LAM_INST_LOAD[]   = "(\\x.\\y.\\z.\\a.\\b.\\c.\\d.\\e.b)";
static const char LAM_INST_STORE[]  = "(\\x.\\y.\\z.\\a.\\b.\\c.\\d.\\e.c)";
static const char LAM_INST_ADDSUB[] = "(\\x.\\y.\\z.\\a.\\b.\\c.\\d.\\e.d)";
static const char LAM_INST_MOV[]    = "(\\x.\\y.\\z.\\a.\\b.\\c.\\d.\\e.e)";
static const char LAM_CMP_EQ[] = "(\\x.(x(\\y.\\z.y)(\\y.\\z.z)(\\y.\\z.z)))";
static const char LAM_CMP_LT[] = "(\\x.(x(\\y.\\z.z)(\\y.\\z.y)(\\y.\\z.z)))";
static const char LAM_CMP_GT[] = "(\\x.(x(\\y.\\z.z)(\\y.\\z.z)(\\y.\\z.y)))";
static const char LAM_CMP_NE[] = "(\\x.(x(\\y.\\z.z)(\\y.\\z.y)(\\y.\\z.y)))";
static const char LAM_CMP_GE[] = "(\\x.(x(\\y.\\z.y)(\\y.\\z.z)(\\y.\\z.y)))";
static const char LAM_CMP_LE[] = "(\\x.(x(\\y.\\z.y)(\\y.\\z.y)(\\y.\\z.z)))";
static const char LAM_IO_GETC[] = "(\\x.\\y.\\z.x)";
static const char LAM_IO_PUTC[] = "(\\x.\\y.\\z.y)";
static const char LAM_IO_EXIT[] = "(\\x.\\y.\\z.z)";
static const char LAM_PLACEHOLDER[] = "(\\x.x)";


static const char* lam_reg(Reg r) {
  switch (r) {
  case A: return LAM_REG_A;
  case B: return LAM_REG_B;
  case C: return LAM_REG_C;
  case D: return LAM_REG_D;
  case BP: return LAM_REG_BP;
  case SP: return LAM_REG_SP;
  default:
    error("unknown register: %d", r);
  }
}

static void lam_print_n (int n, const char* s) {
  for (int i=0; i<n; i++) {
    fputs(s, stdout);
  }
}

static void lam_emit_int(int n) {
#ifndef __eir__
  n &= ((1 << LAM_N_BITS) - 1);
#endif
  fputs(LAM_INT_HEADER, stdout);
  for (int checkbit = 1 << (LAM_N_BITS - 1); checkbit; checkbit >>= 1) {
    fputs("(", stdout);
    fputs((n & checkbit) ? LAM_INT_BIT1 : LAM_INT_BIT0, stdout);
  }
  fputs(LAM_NIL, stdout);
  lam_print_n(LAM_N_BITS, ")");
  fputs(LAM_INT_FOOTER, stdout);
}

static void emit_lam_isimm(Value* v) {
  switch (v->type) {
    case REG: fputs(LAM_NIL, stdout); break;
    case IMM: fputs(LAM_T, stdout); break;
    default:
      error("invalid value");
  }
}

static void emit_lam_value_str(Value* v) {
  switch (v->type) {
    case REG: fputs(lam_reg(v->reg), stdout); break;
    case IMM: lam_emit_int(v->imm); break;
    default:
      error("invalid value");
  }
}

static void lam_emit_inst_header(const char* inst_tag, Value* v) {
  fputs(LAM_CONS_HEADER, stdout);
  fputs(inst_tag, stdout);
  emit_lam_isimm(v);
  emit_lam_value_str(v);
}

static void lam_emit_basic_inst(Inst* inst, const char* inst_tag) {
  lam_emit_inst_header(inst_tag, &inst->src);
  emit_lam_value_str(&inst->dst);
  fputs(LAM_CONS_FOOTER, stdout);
}

static void lam_emit_addsub_inst(Inst* inst, const char* is_add) {
  lam_emit_inst_header(LAM_INST_ADDSUB, &inst->src);
  fputs(LAM_CONS_HEADER, stdout);
  emit_lam_value_str(&inst->dst);
  fputs(is_add, stdout);
  fputs(LAM_CONS_FOOTER, stdout);
  fputs(LAM_CONS_FOOTER, stdout);
}

static void lam_emit_jmpcmp_inst(Inst* inst, const char* cmp_tag) {
  lam_emit_inst_header(LAM_INST_JMPCMP, &inst->src);
  lam_emit_inst_header(cmp_tag, &inst->jmp);
  emit_lam_value_str(&inst->dst);
  fputs(LAM_CONS_FOOTER, stdout);
  fputs(LAM_CONS_FOOTER, stdout);
}

static void lam_emit_cmp_inst(Inst* inst, const char* cmp_tag) {
  lam_emit_inst_header(LAM_INST_CMP, &inst->src);
  fputs(LAM_CONS_HEADER, stdout);
  fputs(cmp_tag, stdout);
  emit_lam_value_str(&inst->dst);
  fputs(LAM_CONS_FOOTER, stdout);
  fputs(LAM_CONS_FOOTER, stdout);
}

static void lam_emit_io_inst(const char* io_tag, Value* v) {
  lam_emit_inst_header(LAM_INST_IO, v);
  fputs(io_tag, stdout);
  fputs(LAM_CONS_FOOTER, stdout);
}

static void lam_emit_exit_inst() {
  fputs(LAM_CONS_HEADER, stdout);
  fputs(LAM_INST_IO, stdout);
  fputs(LAM_NIL, stdout);
  fputs(LAM_NIL, stdout);
  fputs(LAM_IO_EXIT, stdout);
  fputs(LAM_CONS_FOOTER, stdout);
}

static void lam_emit_jmp_inst(Inst* inst) {
  lam_emit_inst_header(LAM_INST_JMP, &inst->jmp);
  fputs(LAM_PLACEHOLDER, stdout);
  fputs(LAM_CONS_FOOTER, stdout);
}

static void lam_emit_dump_inst(void) {
  fputs(LAM_CONS_HEADER, stdout);
  fputs(LAM_INST_MOV, stdout);
  fputs(LAM_NIL, stdout);
  fputs(LAM_NIL, stdout);
  fputs(LAM_NIL, stdout);
  fputs(LAM_CONS_FOOTER, stdout);
}

static void lam_emit_inst(Inst* inst) {
  switch (inst->op) {
  case MOV: lam_emit_basic_inst(inst, LAM_INST_MOV); break;
  case LOAD: lam_emit_basic_inst(inst, LAM_INST_LOAD); break;
  case STORE: lam_emit_basic_inst(inst, LAM_INST_STORE); break;

  case ADD: lam_emit_addsub_inst(inst, LAM_T); break;
  case SUB: lam_emit_addsub_inst(inst, LAM_NIL); break;

  case EQ: lam_emit_cmp_inst(inst, LAM_CMP_EQ); break;
  case NE: lam_emit_cmp_inst(inst, LAM_CMP_NE); break;
  case LT: lam_emit_cmp_inst(inst, LAM_CMP_LT); break;
  case GT: lam_emit_cmp_inst(inst, LAM_CMP_GT); break;
  case LE: lam_emit_cmp_inst(inst, LAM_CMP_LE); break;
  case GE: lam_emit_cmp_inst(inst, LAM_CMP_GE); break;

  case JEQ: lam_emit_jmpcmp_inst(inst, LAM_CMP_EQ); break;
  case JNE: lam_emit_jmpcmp_inst(inst, LAM_CMP_NE); break;
  case JLT: lam_emit_jmpcmp_inst(inst, LAM_CMP_LT); break;
  case JGT: lam_emit_jmpcmp_inst(inst, LAM_CMP_GT); break;
  case JLE: lam_emit_jmpcmp_inst(inst, LAM_CMP_LE); break;
  case JGE: lam_emit_jmpcmp_inst(inst, LAM_CMP_GE); break;

  case JMP: lam_emit_jmp_inst(inst); break;

  case PUTC: lam_emit_io_inst(LAM_IO_PUTC, &inst->src); break;
  case GETC: lam_emit_io_inst(LAM_IO_GETC, &inst->dst); break;

  case EXIT: lam_emit_exit_inst(); break;
  case DUMP: lam_emit_dump_inst(); break;

  default:
    error("oops");
  }
}

static void lam_emit_data_list(Data* data) {
  int n_data;
  for (n_data=0; data; data=data->next, n_data++){
    putchar('\n');
    fputs(LAM_CONS_HEADER, stdout);
    lam_emit_int(data->v);
  }
  fputs(LAM_NIL, stdout);
  lam_print_n(n_data, LAM_CONS_FOOTER);
  putchar('\n');
}

static Inst* lam_emit_chunk(Inst* inst) {
  const int init_pc = inst->pc;
  int n_insts;
  for (n_insts=0; inst && (inst->pc == init_pc); inst=inst->next, n_insts++) {
    fputs("\n  ", stdout);
    fputs(LAM_CONS_HEADER, stdout);
    lam_emit_inst(inst);
  }
  fputs(LAM_NIL, stdout);
  lam_print_n(n_insts, LAM_CONS_FOOTER);
  fputs("\n", stdout);
  return inst;
}

static void lam_emit_text_list(Inst* inst) {
  int n_chunks;
  for (n_chunks=0; inst; n_chunks++) {
    fputs(LAM_CONS_HEADER, stdout);
    inst = lam_emit_chunk(inst);
  }
  fputs(LAM_NIL, stdout);
  lam_print_n(n_chunks, LAM_CONS_FOOTER);
}

void target_lam(Module* module) {
  fputs("(", stdout);
  fputs(LAM_VM, stdout);
  fputs(LAM_8, stdout);
  fputs(LAM_16, stdout);
  lam_emit_data_list(module->data);
  lam_emit_text_list(module->text);
  fputs(")", stdout);
}
