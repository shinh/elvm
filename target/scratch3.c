#include <ir/ir.h>
#include <target/util.h>

#include <stdio.h>
#include <stdlib.h>

static int max(int a, int b) { return a < b ? b : a; }

static const char SCR3_SVG_MD5[] = "fcb8546c50e11d422b78fd55e34d7e7e";
static const char *SCR3_BASE64_TABLE[] = {
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
    "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
    "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m",
    "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z",
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "+", "/"};

typedef int scr3_reg_t;
#define SCR3_REG_MAX (256)
const char *scr3_register_name[SCR3_REG_MAX];
int scr3_reg_count = 0;
static scr3_reg_t scr3_add_new_register(const char *reg_name) {
  if (scr3_reg_count == SCR3_REG_MAX) {
    error("too many register");
  }
  scr3_reg_t scr3_new_reg = scr3_reg_count++;
  scr3_register_name[scr3_new_reg] = reg_name;
  return scr3_new_reg;
}

static scr3_reg_t scr3_reg_a, scr3_reg_b, scr3_reg_c, scr3_reg_d, scr3_reg_sp,
    scr3_reg_bp, scr3_reg_pc, scr3_reg_impl_ret, scr3_reg_impl_page,
    scr3_reg_impl_wait;

enum scr3_block_type {
  SCR3_BLK_WHEN_FLAG_CLICKED,
  SCR3_BLK_SET_VAR,
  SCR3_BLK_CHANGE_VAR,
  SCR3_BLK_ADD,
  SCR3_BLK_SUB,
  SCR3_BLK_MUL,
  SCR3_BLK_DIV,
  SCR3_BLK_MOD,
  SCR3_BLK_DEFINE,
  SCR3_BLK_PROTO,
  SCR3_BLK_CALL,
  SCR3_BLK_ARG_REPORTER,
  SCR3_BLK_IF,
  SCR3_BLK_IF_ELSE,
  SCR3_BLK_FOREVER,
  SCR3_BLK_REPEAT,
  SCR3_BLK_REPEAT_UNTIL,
  SCR3_BLK_WAIT_UNTIL,
  SCR3_BLK_EQUAL,
  SCR3_BLK_NOT,
  SCR3_BLK_LOR,
  SCR3_BLK_LAND,
  SCR3_BLK_GREATER_THAN,
  SCR3_BLK_LESS_THAN,
  SCR3_BLK_JOIN,
  SCR3_BLK_LETTER_OF,
  SCR3_BLK_LENGTH,
  SCR3_BLK_MATHOP,
  SCR3_BLK_SWITCH_BACKDROP,
  SCR3_BLK_BACKDROP_NUM_NAME,
  SCR3_BLK_ADD_TO_LIST,
  SCR3_BLK_DELETE_ALL_LIST,
  SCR3_BLK_DELETE_ITEM,
  SCR3_BLK_REPLACE_ITEM,
  SCR3_BLK_LIST_LENGTH,
  SCR3_BLK_LIST_ITEM,
  SCR3_BLK_ASK_AND_WAIT,
  SCR3_BLK_ANSWER_REPORTER,
  SCR3_BLK_STOPALL,
  SCR3_BLK_DUMMY,
};

enum scr3_list {
  SCR3_LIST_STDOUT,
  SCR3_LIST_STDIN,
  SCR3_LIST_MEMORY,
  SCR3_LIST_PAGE,
  SCR3_NUM_OF_LIST
};
const char *SCR3_LIST_NAME[] = {"stdout", "stdin", "memory", "page"};

enum scr3_src_type {
  SCR3_SRC_IMMEDIATE,
  SCR3_SRC_REGISTER,
  SCR3_SRC_BLOCK,
  SCR3_SRC_EMPTY,
};

struct scr3_block;
struct scr3_src {
  enum scr3_src_type type;
  union {
    const char *value;
    scr3_reg_t reg;
    struct scr3_block *block;
  };
};

struct scr3_block {
  int id;
  const char *opcode_str;
  struct scr3_block *prev;
  struct scr3_block *next;
  enum scr3_block_type type;
  bool is_shadow;
  bool is_toplevel;
  union {
    struct scr3_src src;
    struct scr3_block *blk;
    struct {
      struct scr3_src lhs;
      struct scr3_src rhs;
    } bin_op;
    struct {
      struct scr3_block *lhs;
      struct scr3_block *rhs;
    } bin_cond;
    struct {
      struct scr3_block *cond;
      struct scr3_block *substack_then;
      struct scr3_block *substack_else;
    } if_else;
    struct {
      const char *name;
      struct scr3_block *reporter;
    } arg;
    struct {
      struct scr3_src times;
      struct scr3_block *cond;
      struct scr3_block *substack;
    } repeat;
  } input;
  union {
    scr3_reg_t reg;
    enum scr3_list list;
    const char *mathop;
    const char *reporter_name;
  } field;
  struct {
    const char *proccode;
  } mutation;
  struct scr3_block *list_next;
};

static struct scr3_block *scr3_last_block_p = NULL;

static const struct scr3_src EMPTY_SRC =
    (struct scr3_src){.type = SCR3_SRC_EMPTY};

// =============================================================================
// Prototypes
// =============================================================================

//------------------------------------------------------------------------------
// Helper functions
//------------------------------------------------------------------------------

// set type-specific data
// return 0 for success
static int scr3_set_block_info(enum scr3_block_type type,
                               const char **opcode_str_p, bool *is_toplevel_p,
                               bool *is_shadow_p);

static struct scr3_src scr3_src_imm(const char *value);
static struct scr3_src scr3_src_reg(scr3_reg_t reg);
static struct scr3_src scr3_src_block(struct scr3_block *block);
static void scr3_src_set_parent(struct scr3_src src, struct scr3_block *parent);

static struct scr3_block *scr3_create_block(enum scr3_block_type type,
                                            struct scr3_block *prev,
                                            struct scr3_block *next);

// -----------------------------------------------------------------------------
// Builder functions
// -----------------------------------------------------------------------------

static struct scr3_src scr3_build_add(struct scr3_src lhs, struct scr3_src rhs);
static struct scr3_src scr3_build_sub(struct scr3_src lhs, struct scr3_src rhs);
static struct scr3_src scr3_build_mul(struct scr3_src lhs, struct scr3_src rhs);
static struct scr3_src scr3_build_div(struct scr3_src lhs, struct scr3_src rhs);
static struct scr3_src scr3_build_mod(struct scr3_src lhs, struct scr3_src rhs);
static struct scr3_src scr3_build_join(struct scr3_src s1, struct scr3_src s2);
static struct scr3_src scr3_build_letter_of(struct scr3_src idx,
                                            struct scr3_src str);
static struct scr3_src scr3_build_length(struct scr3_src str);

static struct scr3_block *scr3_build_equal(struct scr3_src lhs,
                                           struct scr3_src rhs);
static struct scr3_block *scr3_build_gt(struct scr3_src lhs,
                                        struct scr3_src rhs);
static struct scr3_block *scr3_build_lt(struct scr3_src lhs,
                                        struct scr3_src rhs);
static struct scr3_block *scr3_build_not(struct scr3_block *cond);
static struct scr3_block *scr3_build_or(struct scr3_block *lhs,
                                        struct scr3_block *rhs);

static struct scr3_block *scr3_build_when_flag_clicked(void);
static struct scr3_block *scr3_build_set_var(struct scr3_block *parent,
                                             scr3_reg_t reg,
                                             struct scr3_src src);
static struct scr3_block *scr3_build_change_var(struct scr3_block *parent,
                                                scr3_reg_t reg,
                                                struct scr3_src src);
static struct scr3_src scr3_build_arg_reporter(const char *arg_name);

// If this procedure takes an argument, proc_name must contain "%s" like
// "sample_proc %s"
static struct scr3_block *scr3_build_define(const char *proc_name,
                                            const char *argument_name);
static struct scr3_block *scr3_build_call(struct scr3_block *parent,
                                          const char *proc_name,
                                          struct scr3_src argument);
static struct scr3_block *scr3_build_if(struct scr3_block *parent,
                                        struct scr3_block *cond,
                                        struct scr3_block *substack_then);
static struct scr3_block *scr3_build_if_else(struct scr3_block *parent,
                                             struct scr3_block *cond,
                                             struct scr3_block *substack_then,
                                             struct scr3_block *substack_else);
static struct scr3_block *scr3_build_forever(struct scr3_block *parent,
                                             struct scr3_block *substack);
static struct scr3_block *scr3_build_repeat(struct scr3_block *parent,
                                            struct scr3_src times,
                                            struct scr3_block *substack);
static struct scr3_block *scr3_build_repeat_until(struct scr3_block *parent,
                                                  struct scr3_block *cond,
                                                  struct scr3_block *substack);
static struct scr3_block *scr3_build_wait_until(struct scr3_block *parent,
                                                struct scr3_block *cond);
static struct scr3_src scr3_build_mathop(struct scr3_src num,
                                         const char *operator_name);
static struct scr3_block *scr3_build_switch_backdrop(struct scr3_block *parent,
                                                     struct scr3_src backdrop);
static struct scr3_src scr3_build_backdrop_reporter(bool to_get_name);
static struct scr3_block *scr3_build_add_to_list(struct scr3_block *parent,
                                                 struct scr3_src elem,
                                                 enum scr3_list list);
static struct scr3_block *scr3_build_delete_all_of_list(
    struct scr3_block *parent, enum scr3_list list);
static struct scr3_block *scr3_build_delete_item(struct scr3_block *parent,
                                                 struct scr3_src src,
                                                 enum scr3_list list);
static struct scr3_block *scr3_build_replace_item(struct scr3_block *parent,
                                                  struct scr3_src index,
                                                  struct scr3_src item,
                                                  enum scr3_list list);
static struct scr3_src scr3_build_length_of_list(enum scr3_list list);
static struct scr3_src scr3_build_item_of_list(struct scr3_src index,
                                               enum scr3_list list);
static struct scr3_block *scr3_build_ask_and_wait(struct scr3_block *parent,
                                                  struct scr3_src question);
static struct scr3_src scr3_build_answer_reporter(void);
static struct scr3_block *scr3_build_stop_all(struct scr3_block *parent);

// -----------------------------------------------------------------------------
// Emitter functions
// -----------------------------------------------------------------------------

static struct scr3_block *scr3_emit_input_src(const char *field_name,
                                              struct scr3_src src);
static void scr3_emit_field_register(const char *field_name, scr3_reg_t reg);
static void scr3_emit_field_list(const char *field_name, enum scr3_list list);
static void scr3_emit_block(struct scr3_block *b);
static void scr3_emit_all(void);

// -----------------------------------------------------------------------------
// Runtime implementation
// -----------------------------------------------------------------------------

static void scr3_impl_ascii_encode(void);
static void scr3_impl_ascii_decode(void);
static void scr3_impl_base64_encode(void);
static void scr3_impl_base64_decode(void);
static void scr3_impl_writeback_page(void);
static void scr3_impl_expand_page(void);
static void scr3_impl_store(void);
static void scr3_impl_load(void);
static void scr3_impl_getchar(void);
static void scr3_impl_putchar(void);
static void scr3_impl_read_stdin(void);
static void scr3_impl_initialize(void);

// -----------------------------------------------------------------------------
// Control flow
// -----------------------------------------------------------------------------

static struct scr3_src src_convert(Value value);
static void scr3_emit_inst(Inst *inst, struct scr3_block **p);
static struct scr3_block *scr3_impl_switch_helper(scr3_reg_t reg, int lval,
                                                  int hval,
                                                  struct scr3_block *cases[],
                                                  int cases_size);
static struct scr3_block *scr3_impl_chunk(int low_pc, int high_pc,
                                          struct scr3_block *each_pc[],
                                          int max_pc);
static struct scr3_block *scr3_impl_chunk_switch(struct scr3_block *parent,
                                                 struct scr3_block *each_pc[],
                                                 int each_pc_size);
static void scr3_impl_main_loop(Inst *inst);
static void scr3_impl_init_memory(Data *data);
void target_scratch3(Module *module);

// =============================================================================
// Definitions
// =============================================================================

//------------------------------------------------------------------------------
// Helper functions
//------------------------------------------------------------------------------

// set type-specific data
// return 0 for success
static int scr3_set_block_info(enum scr3_block_type type,
                               const char **opcode_str_p, bool *is_toplevel_p,
                               bool *is_shadow_p) {
#define GEN_CASE(type_case, opcode, toplevel, shadow) \
  case type_case: {                                   \
    *opcode_str_p = opcode;                           \
    *is_toplevel_p = toplevel;                        \
    *is_shadow_p = shadow;                            \
    return 0;                                         \
  }

  switch (type) {
    GEN_CASE(SCR3_BLK_WHEN_FLAG_CLICKED, "event_whenflagclicked", true, false);
    GEN_CASE(SCR3_BLK_SET_VAR, "data_setvariableto", false, false);
    GEN_CASE(SCR3_BLK_CHANGE_VAR, "data_changevariableby", false, false);
    GEN_CASE(SCR3_BLK_ADD, "operator_add", false, false);
    GEN_CASE(SCR3_BLK_SUB, "operator_subtract", false, false);
    GEN_CASE(SCR3_BLK_MUL, "operator_multiply", false, false);
    GEN_CASE(SCR3_BLK_DIV, "operator_divide", false, false);
    GEN_CASE(SCR3_BLK_MOD, "operator_mod", false, false);
    GEN_CASE(SCR3_BLK_DEFINE, "procedures_definition", true, false);
    GEN_CASE(SCR3_BLK_PROTO, "procedures_prototype", false, true);
    GEN_CASE(SCR3_BLK_CALL, "procedures_call", false, false);
    GEN_CASE(SCR3_BLK_ARG_REPORTER, "argument_reporter_string_number", false,
             false);
    GEN_CASE(SCR3_BLK_IF, "control_if", false, false);
    GEN_CASE(SCR3_BLK_IF_ELSE, "control_if_else", false, false);
    GEN_CASE(SCR3_BLK_FOREVER, "control_forever", false, false);
    GEN_CASE(SCR3_BLK_REPEAT, "control_repeat", false, false);
    GEN_CASE(SCR3_BLK_REPEAT_UNTIL, "control_repeat_until", false, false);
    GEN_CASE(SCR3_BLK_WAIT_UNTIL, "control_wait_until", false, false);
    GEN_CASE(SCR3_BLK_EQUAL, "operator_equals", false, false);
    GEN_CASE(SCR3_BLK_NOT, "operator_not", false, false);
    GEN_CASE(SCR3_BLK_LOR, "operator_or", false, false);
    GEN_CASE(SCR3_BLK_LAND, "operator_and", false, false);
    GEN_CASE(SCR3_BLK_GREATER_THAN, "operator_gt", false, false);
    GEN_CASE(SCR3_BLK_LESS_THAN, "operator_lt", false, false);
    GEN_CASE(SCR3_BLK_JOIN, "operator_join", false, false);
    GEN_CASE(SCR3_BLK_LENGTH, "operator_length", false, false);
    GEN_CASE(SCR3_BLK_LETTER_OF, "operator_letter_of", false, false);
    GEN_CASE(SCR3_BLK_MATHOP, "operator_mathop", false, false);
    GEN_CASE(SCR3_BLK_SWITCH_BACKDROP, "looks_switchbackdropto", false, false);
    GEN_CASE(SCR3_BLK_BACKDROP_NUM_NAME, "looks_backdropnumbername", false,
             false);
    GEN_CASE(SCR3_BLK_ADD_TO_LIST, "data_addtolist", false, false);
    GEN_CASE(SCR3_BLK_DELETE_ALL_LIST, "data_deletealloflist", false, false);
    GEN_CASE(SCR3_BLK_DELETE_ITEM, "data_deleteoflist", false, false);
    GEN_CASE(SCR3_BLK_REPLACE_ITEM, "data_replaceitemoflist", false, false);
    GEN_CASE(SCR3_BLK_LIST_LENGTH, "data_lengthoflist", false, false);
    GEN_CASE(SCR3_BLK_LIST_ITEM, "data_itemoflist", false, false);
    GEN_CASE(SCR3_BLK_ASK_AND_WAIT, "sensing_askandwait", false, false);
    GEN_CASE(SCR3_BLK_ANSWER_REPORTER, "sensing_answer", false, false);
    GEN_CASE(SCR3_BLK_STOPALL, "control_stop", false, false);
    case SCR3_BLK_DUMMY:
      return 0;
  }
#undef GEN_CASE
  return 1;
}

static struct scr3_src scr3_src_imm(const char *value) {
  return (struct scr3_src){.type = SCR3_SRC_IMMEDIATE, .value = value};
}
static struct scr3_src scr3_src_reg(scr3_reg_t reg) {
  return (struct scr3_src){.type = SCR3_SRC_REGISTER, .reg = reg};
}
static struct scr3_src scr3_src_block(struct scr3_block *block) {
  return (struct scr3_src){.type = SCR3_SRC_BLOCK, .block = block};
}
static void scr3_src_set_parent(struct scr3_src src,
                                struct scr3_block *parent) {
  if (src.type == SCR3_SRC_BLOCK) {
    src.block->prev = parent;
  }
}

static struct scr3_block *scr3_create_block(enum scr3_block_type type,
                                            struct scr3_block *prev,
                                            struct scr3_block *next) {
  struct scr3_block *b;
  if (prev && prev->type == SCR3_BLK_DUMMY) {
    b = prev;
  } else {
    b = malloc(sizeof(struct scr3_block));
    b->list_next = scr3_last_block_p;
    scr3_last_block_p = b;
  }

  b->type = type;
  b->prev = prev;
  if (prev) prev->next = b;
  b->next = next;
  if (next) next->prev = b;

  static int next_block_id = 0;
  b->id = next_block_id++;
  if (scr3_set_block_info(type, &b->opcode_str, &b->is_toplevel,
                          &b->is_shadow)) {
    error("wrong block type");
  }
  return b;
}

// -----------------------------------------------------------------------------
// Builder functions
// -----------------------------------------------------------------------------

#define GEN_BUILD_BINOP(func_name, block_type)                                 \
  static struct scr3_src func_name(struct scr3_src lhs, struct scr3_src rhs) { \
    struct scr3_block *b = scr3_create_block(block_type, NULL, NULL);          \
    scr3_src_set_parent(lhs, b);                                               \
    scr3_src_set_parent(rhs, b);                                               \
    b->input.bin_op.lhs = lhs;                                                 \
    b->input.bin_op.rhs = rhs;                                                 \
    return (struct scr3_src){.type = SCR3_SRC_BLOCK, .block = b};              \
  }
GEN_BUILD_BINOP(scr3_build_add, SCR3_BLK_ADD)
GEN_BUILD_BINOP(scr3_build_sub, SCR3_BLK_SUB)
GEN_BUILD_BINOP(scr3_build_mul, SCR3_BLK_MUL)
GEN_BUILD_BINOP(scr3_build_div, SCR3_BLK_DIV)
GEN_BUILD_BINOP(scr3_build_mod, SCR3_BLK_MOD)

GEN_BUILD_BINOP(scr3_build_join, SCR3_BLK_JOIN)
GEN_BUILD_BINOP(scr3_build_letter_of, SCR3_BLK_LETTER_OF)
#undef GEN_BUILD_BINOP

#define GEN_BUILD_BOOL_BINOP(func_name, block_type)                   \
  static struct scr3_block *func_name(struct scr3_src lhs,            \
                                      struct scr3_src rhs) {          \
    struct scr3_block *b = scr3_create_block(block_type, NULL, NULL); \
    scr3_src_set_parent(lhs, b);                                      \
    scr3_src_set_parent(rhs, b);                                      \
    b->input.bin_op.lhs = lhs;                                        \
    b->input.bin_op.rhs = rhs;                                        \
    return b;                                                         \
  }
GEN_BUILD_BOOL_BINOP(scr3_build_equal, SCR3_BLK_EQUAL)
GEN_BUILD_BOOL_BINOP(scr3_build_gt, SCR3_BLK_GREATER_THAN)
GEN_BUILD_BOOL_BINOP(scr3_build_lt, SCR3_BLK_LESS_THAN)
#undef GEN_BUILD_BOOL_BINOP

static struct scr3_src scr3_build_length(struct scr3_src src) {
  struct scr3_block *b = scr3_create_block(SCR3_BLK_LENGTH, NULL, NULL);
  scr3_src_set_parent(src, b);
  b->input.src = src;
  return scr3_src_block(b);
}

static struct scr3_block *scr3_build_not(struct scr3_block *cond) {
  struct scr3_block *b = scr3_create_block(SCR3_BLK_NOT, NULL, NULL);
  cond->prev = b;
  b->input.blk = cond;
  return b;
}

static struct scr3_block *scr3_build_or(struct scr3_block *lhs,
                                        struct scr3_block *rhs) {
  struct scr3_block *b = scr3_create_block(SCR3_BLK_LOR, NULL, NULL);
  lhs->prev = b;
  rhs->prev = b;
  b->input.bin_cond.lhs = lhs;
  b->input.bin_cond.rhs = rhs;
  return b;
}

static struct scr3_block *scr3_build_when_flag_clicked(void) {
  return scr3_create_block(SCR3_BLK_WHEN_FLAG_CLICKED, NULL, NULL);
}

static struct scr3_block *scr3_build_set_var(struct scr3_block *parent,
                                             scr3_reg_t reg,
                                             struct scr3_src src) {
  struct scr3_block *b = scr3_create_block(SCR3_BLK_SET_VAR, parent, NULL);
  scr3_src_set_parent(src, b);
  b->input.src = src;
  b->field.reg = reg;
  return b;
}

static struct scr3_block *scr3_build_change_var(struct scr3_block *parent,
                                                scr3_reg_t reg,
                                                struct scr3_src src) {
  struct scr3_block *b = scr3_create_block(SCR3_BLK_CHANGE_VAR, parent, NULL);
  scr3_src_set_parent(src, b);
  b->input.src = src;
  b->field.reg = reg;
  return b;
}

static struct scr3_src scr3_build_arg_reporter(const char *arg_name) {
  struct scr3_block *b = scr3_create_block(SCR3_BLK_ARG_REPORTER, NULL, NULL);
  b->field.reporter_name = arg_name;
  return scr3_src_block(b);
}

// If this procedure takes an argument, proc_name must contain "%s" like
// "sample_proc %s"
static struct scr3_block *scr3_build_define(const char *proc_name,
                                            const char *argument_name) {
  struct scr3_block *def = scr3_create_block(SCR3_BLK_DEFINE, NULL, NULL);
  struct scr3_block *proto = scr3_create_block(SCR3_BLK_PROTO, NULL, NULL);
  proto->prev = def;
  proto->mutation.proccode = proc_name;
  def->input.blk = proto;

  if (argument_name) {
    struct scr3_src tmp = scr3_build_arg_reporter(argument_name);
    struct scr3_block *arg_reporter = tmp.block;
    arg_reporter->prev = proto;
    arg_reporter->is_shadow = true;
    proto->input.arg.name = argument_name;
    proto->input.arg.reporter = arg_reporter;
  } else {
    proto->input.arg.name = NULL;
    proto->input.arg.reporter = NULL;
  }
  return def;
}
static struct scr3_block *scr3_build_call(struct scr3_block *parent,
                                          const char *proc_name,
                                          struct scr3_src argument) {
  struct scr3_block *b = scr3_create_block(SCR3_BLK_CALL, parent, NULL);
  scr3_src_set_parent(argument, b);
  b->mutation.proccode = proc_name;
  b->input.src = argument;
  return b;
}

static struct scr3_block *scr3_build_if(struct scr3_block *parent,
                                        struct scr3_block *cond,
                                        struct scr3_block *substack_then) {
  struct scr3_block *b = scr3_create_block(SCR3_BLK_IF, parent, NULL);
  cond->prev = b;
  if (substack_then) substack_then->prev = b;
  b->input.if_else.cond = cond;
  b->input.if_else.substack_then = substack_then;
  return b;
}
static struct scr3_block *scr3_build_if_else(struct scr3_block *parent,
                                             struct scr3_block *cond,
                                             struct scr3_block *substack_then,
                                             struct scr3_block *substack_else) {
  struct scr3_block *b = scr3_create_block(SCR3_BLK_IF_ELSE, parent, NULL);
  cond->prev = b;
  if (substack_then) substack_then->prev = b;
  if (substack_else) substack_else->prev = b;
  b->input.if_else.cond = cond;
  b->input.if_else.substack_then = substack_then;
  b->input.if_else.substack_else = substack_else;
  return b;
}

static struct scr3_block *scr3_build_forever(struct scr3_block *parent,
                                             struct scr3_block *substack) {
  struct scr3_block *b = scr3_create_block(SCR3_BLK_FOREVER, parent, NULL);
  b->input.repeat.substack = substack;
  substack->prev = b;
  return b;
}
static struct scr3_block *scr3_build_repeat(struct scr3_block *parent,
                                            struct scr3_src times,
                                            struct scr3_block *substack) {
  struct scr3_block *b = scr3_create_block(SCR3_BLK_REPEAT, parent, NULL);
  scr3_src_set_parent(times, b);
  b->input.repeat.times = times;
  b->input.repeat.substack = substack;
  substack->prev = b;
  return b;
}
static struct scr3_block *scr3_build_repeat_until(struct scr3_block *parent,
                                                  struct scr3_block *cond,
                                                  struct scr3_block *substack) {
  struct scr3_block *b = scr3_create_block(SCR3_BLK_REPEAT_UNTIL, parent, NULL);
  b->input.repeat.cond = cond;
  cond->prev = b;
  b->input.repeat.substack = substack;
  substack->prev = b;
  return b;
}

static struct scr3_block *scr3_build_wait_until(struct scr3_block *parent,
                                                struct scr3_block *cond) {
  struct scr3_block *b = scr3_create_block(SCR3_BLK_WAIT_UNTIL, parent, NULL);
  b->input.blk = cond;
  return b;
}

static struct scr3_src scr3_build_mathop(struct scr3_src num,
                                         const char *operator_name) {
  struct scr3_block *b = scr3_create_block(SCR3_BLK_MATHOP, NULL, NULL);
  b->input.src = num;
  b->field.mathop = operator_name;
  return scr3_src_block(b);
}

static struct scr3_block *scr3_build_switch_backdrop(struct scr3_block *parent,
                                                     struct scr3_src backdrop) {
  struct scr3_block *b =
      scr3_create_block(SCR3_BLK_SWITCH_BACKDROP, parent, NULL);
  scr3_src_set_parent(backdrop, b);
  b->input.src = backdrop;
  return b;
}

static struct scr3_src scr3_build_backdrop_reporter(bool to_get_name) {
  struct scr3_block *b =
      scr3_create_block(SCR3_BLK_BACKDROP_NUM_NAME, NULL, NULL);
  if (to_get_name) {
    b->field.reporter_name = "name";
  } else {
    b->field.reporter_name = "number";
  }
  return scr3_src_block(b);
}

static struct scr3_block *scr3_build_add_to_list(struct scr3_block *parent,
                                                 struct scr3_src elem,
                                                 enum scr3_list list) {
  struct scr3_block *b = scr3_create_block(SCR3_BLK_ADD_TO_LIST, parent, NULL);
  scr3_src_set_parent(elem, b);
  b->input.src = elem;
  b->field.list = list;
  return b;
}

static struct scr3_block *scr3_build_delete_all_of_list(
    struct scr3_block *parent, enum scr3_list list) {
  struct scr3_block *b =
      scr3_create_block(SCR3_BLK_DELETE_ALL_LIST, parent, NULL);
  b->field.list = list;
  return b;
}
static struct scr3_block *scr3_build_delete_item(struct scr3_block *parent,
                                                 struct scr3_src src,
                                                 enum scr3_list list) {
  struct scr3_block *b = scr3_create_block(SCR3_BLK_DELETE_ITEM, parent, NULL);
  scr3_src_set_parent(src, b);
  b->input.src = src;
  b->field.list = list;
  return b;
}

static struct scr3_block *scr3_build_replace_item(struct scr3_block *parent,
                                                  struct scr3_src index,
                                                  struct scr3_src item,
                                                  enum scr3_list list) {
  struct scr3_block *b = scr3_create_block(SCR3_BLK_REPLACE_ITEM, parent, NULL);
  scr3_src_set_parent(index, b);
  scr3_src_set_parent(item, b);
  b->input.bin_op.lhs = index;
  b->input.bin_op.rhs = item;
  b->field.list = list;
  return b;
}

static struct scr3_src scr3_build_length_of_list(enum scr3_list list) {
  struct scr3_block *b = scr3_create_block(SCR3_BLK_LIST_LENGTH, NULL, NULL);
  b->field.list = list;
  return scr3_src_block(b);
}

static struct scr3_src scr3_build_item_of_list(struct scr3_src index,
                                               enum scr3_list list) {
  struct scr3_block *b = scr3_create_block(SCR3_BLK_LIST_ITEM, NULL, NULL);
  scr3_src_set_parent(index, b);
  b->input.src = index;
  b->field.list = list;
  return scr3_src_block(b);
}

static struct scr3_block *scr3_build_ask_and_wait(struct scr3_block *parent,
                                                  struct scr3_src question) {
  struct scr3_block *b = scr3_create_block(SCR3_BLK_ASK_AND_WAIT, parent, NULL);
  scr3_src_set_parent(question, b);
  b->input.src = question;
  return b;
}
static struct scr3_src scr3_build_answer_reporter(void) {
  struct scr3_block *b =
      scr3_create_block(SCR3_BLK_ANSWER_REPORTER, NULL, NULL);
  return scr3_src_block(b);
}

static struct scr3_block *scr3_build_stop_all(struct scr3_block *parent) {
  struct scr3_block *b = scr3_create_block(SCR3_BLK_STOPALL, parent, NULL);
  return b;
}

// -----------------------------------------------------------------------------
// Emitter functions
// -----------------------------------------------------------------------------

static struct scr3_block *scr3_emit_input_src(const char *field_name,
                                              struct scr3_src src) {
  switch (src.type) {
    case SCR3_SRC_IMMEDIATE:
      printf("\"%s\":[1,[10,\"%s\"]]", field_name, src.value);
      return NULL;
    case SCR3_SRC_REGISTER: {
      const char *reg = scr3_register_name[src.reg];
      printf("\"%s\":[2,[12,\"%s\",\"#r:%s\"]]", field_name, reg, reg);
      return NULL;
    }
    case SCR3_SRC_BLOCK:
      if (src.block) {
        printf("\"%s\":[2,\"b%d\"]", field_name, src.block->id);
      } else {
        printf("\"%s\":[2,null]", field_name);
      }
      return src.block;
    case SCR3_SRC_EMPTY:
      error("cannot emit empty src");
      return NULL;
  }
  return NULL;
}

static void scr3_emit_field_register(const char *field_name, scr3_reg_t reg) {
  printf("\"%s\":[\"%s\",\"#r:%s\"]", field_name, scr3_register_name[reg],
         scr3_register_name[reg]);
}
static void scr3_emit_field_list(const char *field_name, enum scr3_list list) {
  printf("\"%s\":[\"%s\",\"#l:%s\"]", field_name, SCR3_LIST_NAME[list],
         SCR3_LIST_NAME[list]);
}

static void scr3_emit_block(struct scr3_block *b) {
  printf("\"b%d\":{", b->id);
  printf("\"opcode\":\"%s\"", b->opcode_str);
  if (b->next)
    printf(",\"next\":\"b%d\"", b->next->id);
  else
    printf(",\"next\":null");
  if (b->prev)
    printf(",\"parent\":\"b%d\"", b->prev->id);
  else
    printf(",\"parent\":null");
  if (b->is_shadow)
    printf(",\"shadow\":true");
  else
    printf(",\"shadow\":false");
  if (b->is_toplevel)
    printf(",\"topLevel\":true,\"x\":0,\"y\":0");
  else
    printf(",\"topLevel\":false");

  switch (b->type) {
    case SCR3_BLK_WHEN_FLAG_CLICKED:
      printf(",\"inputs\":{},\"fields\":{}");
      break;
    case SCR3_BLK_SET_VAR:
    case SCR3_BLK_CHANGE_VAR:
      printf(",\"inputs\":{");
      scr3_emit_input_src("VALUE", b->input.src);
      printf("},\"fields\":{");
      scr3_emit_field_register("VARIABLE", b->field.reg);
      printf("}");
      break;
    case SCR3_BLK_ADD:
    case SCR3_BLK_SUB:
    case SCR3_BLK_MUL:
    case SCR3_BLK_DIV:
    case SCR3_BLK_MOD:
      printf(",\"inputs\":{");
      scr3_emit_input_src("NUM1", b->input.bin_op.lhs);
      printf(",");
      scr3_emit_input_src("NUM2", b->input.bin_op.rhs);
      printf("},\"fields\":{}");
      break;
    case SCR3_BLK_DEFINE:
      printf(",\"inputs\":{");
      printf("\"custom_block\":[1, \"b%d\"]", b->input.blk->id);
      printf("},\"fields\":{}");
      break;
    case SCR3_BLK_PROTO:
      if (b->input.arg.reporter) {
        // have argument
        printf(",\"inputs\":{\"arg01\":[1,\"b%d\"]},\"fields\":{}",
               b->input.arg.reporter->id);
        printf(
            ",\"mutation\":{\"tagName\":\"mutation\",\"children\":[],"
            "\"proccode\":"
            "\"%s\",\"warp\":true,\"argumentids\":\"[\\\"arg01\\\"]\","
            "\"argumentnames\":\"[\\\"%s\\\"]\",\"argumentdefaults\":\"["
            "\\\"\\\"]\"}",
            b->mutation.proccode, b->input.arg.name);
      } else {
        // have no argument
        printf(",\"inputs\":{},\"fields\":{}");
        printf(
            ",\"mutation\":{\"tagName\":\"mutation\",\"children\":[],"
            "\"proccode\":"
            "\"%s\",\"warp\":true,\"argumentids\":\"[]\",\"argumentnames\":\"[]"
            "\","
            "\"argumentdefaults\":\"[]\"}",
            b->mutation.proccode);
      }
      break;
    case SCR3_BLK_CALL:
      if (b->input.src.type == SCR3_SRC_EMPTY) {
        printf(
            ",\"inputs\":{},\"fields\":{},\"mutation\":{\"tagName\":"
            "\"mutation\","
            "\"children\":[],\"proccode\":\"%s\",\"warp\":true,\"argumentids\":"
            "\"[]\"}",
            b->mutation.proccode);
      } else {
        printf(",\"inputs\":{");
        scr3_emit_input_src("arg01", b->input.src);
        printf(
            "},\"fields\":{},\"mutation\":{\"tagName\":\"mutation\","
            "\"children\":[],"
            "\"proccode\":\"%s\",\"warp\":true,\"argumentids\":\"["
            "\\\"arg01\\\"]\"}",
            b->mutation.proccode);
      }
      break;
    case SCR3_BLK_ARG_REPORTER:
      printf(",\"inputs\":{},\"fields\":{\"VALUE\":[\"%s\", null]}",
             b->field.reporter_name);
      break;

    case SCR3_BLK_FOREVER:
      printf(",\"inputs\":{\"SUBSTACK\":[2,\"b%d\"]},\"fields\":{}",
             b->input.repeat.substack->id);
      break;
    case SCR3_BLK_REPEAT:
      printf(",\"inputs\":{");
      scr3_emit_input_src("TIMES", b->input.repeat.times);
      printf(",\"SUBSTACK\":[2,\"b%d\"]},\"fields\":{}",
             b->input.repeat.substack->id);
      break;
    case SCR3_BLK_REPEAT_UNTIL:
      printf(
          ",\"inputs\":{\"CONDITION\":[2,\"b%d\"],\"SUBSTACK\":[2,\"b%d\"]},"
          "\"fields\":{}",
          b->input.repeat.cond->id, b->input.repeat.substack->id);
      break;
    case SCR3_BLK_WAIT_UNTIL:
      printf(",\"inputs\":{\"CONDITION\":[2,\"b%d\"]},\"fields\":{}",
             b->input.blk->id);
      break;
    case SCR3_BLK_IF:
      printf(
          ",\"inputs\":{\"CONDITION\":[2,\"b%d\"],\"SUBSTACK\":[2,\"b%d\"]},"
          "\"fields\":{}",
          b->input.if_else.cond->id, b->input.if_else.substack_then->id);
      break;
    case SCR3_BLK_IF_ELSE:
      printf(",\"inputs\":{\"CONDITION\":[2,\"b%d\"]",
             b->input.if_else.cond->id);
      if (b->input.if_else.substack_then)
        printf(",\"SUBSTACK\":[2,\"b%d\"]", b->input.if_else.substack_then->id);
      else
        printf(",\"SUBSTACK\":[1,null]");
      if (b->input.if_else.substack_else)
        printf(",\"SUBSTACK2\":[2,\"b%d\"]",
               b->input.if_else.substack_else->id);
      else
        printf(",\"SUBSTACK2\":[1,null]");
      printf("},\"fields\":{}");
      break;
    case SCR3_BLK_EQUAL:
    case SCR3_BLK_GREATER_THAN:
    case SCR3_BLK_LESS_THAN:
      printf(",\"inputs\":{");
      scr3_emit_input_src("OPERAND1", b->input.bin_op.lhs);
      printf(",");
      scr3_emit_input_src("OPERAND2", b->input.bin_op.rhs);
      printf("},\"fields\":{}");
      break;
    case SCR3_BLK_NOT:
      printf(",\"inputs\":{\"OPERAND\":[2,\"b%d\"]},\"fields\":{}",
             b->input.blk->id);
      break;
    case SCR3_BLK_JOIN:
      printf(",\"inputs\":{");
      scr3_emit_input_src("STRING1", b->input.bin_op.lhs);
      printf(",");
      scr3_emit_input_src("STRING2", b->input.bin_op.rhs);
      printf("},\"fields\":{}");
      break;
    case SCR3_BLK_LOR:
    case SCR3_BLK_LAND:
      printf(
          ",\"inputs\":{\"OPERAND1\":[2,\"b%d\"],\"OPERAND2\":[2,\"b%d\"]},"
          "\"fields\":{}",
          b->input.bin_cond.lhs->id, b->input.bin_cond.rhs->id);
      break;
    case SCR3_BLK_LETTER_OF:
      printf(",\"inputs\":{");
      scr3_emit_input_src("LETTER", b->input.bin_op.lhs);
      printf(",");
      scr3_emit_input_src("STRING", b->input.bin_op.rhs);
      printf("},\"fields\":{}");
      break;
    case SCR3_BLK_LENGTH:
      printf(",\"inputs\":{");
      scr3_emit_input_src("STRING", b->input.src);
      printf("},\"fields\":{}");
      break;
    case SCR3_BLK_MATHOP:
      printf(",\"inputs\":{");
      scr3_emit_input_src("NUM", b->input.src);
      printf("},\"fields\":{\"OPERATOR\":[\"%s\", null]}", b->field.mathop);
      break;
    case SCR3_BLK_SWITCH_BACKDROP:
      printf(",\"inputs\":{");
      scr3_emit_input_src("BACKDROP", b->input.src);
      printf("},\"fields\":{}");
      break;
    case SCR3_BLK_BACKDROP_NUM_NAME:
      printf(",\"inputs\":{}");
      printf(",\"fields\":{\"NUMBER_NAME\":[\"%s\",null]}",
             b->field.reporter_name);
      break;
    case SCR3_BLK_ADD_TO_LIST:
      printf(",\"inputs\":{");
      scr3_emit_input_src("ITEM", b->input.src);
      printf("},\"fields\":{");
      scr3_emit_field_list("LIST", b->field.list);
      printf("}");
      break;
    case SCR3_BLK_DELETE_ALL_LIST:
      printf(",\"inputs\":{},\"fields\":{");
      scr3_emit_field_list("LIST", b->field.list);
      printf("}");
      break;
    case SCR3_BLK_DELETE_ITEM:
      printf(",\"inputs\":{");
      scr3_emit_input_src("INDEX", b->input.src);
      printf("},\"fields\":{");
      scr3_emit_field_list("LIST", b->field.list);
      printf("}");
      break;
    case SCR3_BLK_REPLACE_ITEM:
      printf(",\"inputs\":{");
      scr3_emit_input_src("INDEX", b->input.bin_op.lhs);
      printf(",");
      scr3_emit_input_src("ITEM", b->input.bin_op.rhs);
      printf("},\"fields\":{");
      scr3_emit_field_list("LIST", b->field.list);
      printf("}");
      break;
    case SCR3_BLK_LIST_LENGTH:
      printf(",\"inputs\":{},\"fields\":{");
      scr3_emit_field_list("LIST", b->field.list);
      printf("}");
      break;
    case SCR3_BLK_LIST_ITEM:
      printf(",\"inputs\":{");
      scr3_emit_input_src("INDEX", b->input.bin_op.lhs);
      printf("},\"fields\":{");
      scr3_emit_field_list("LIST", b->field.list);
      printf("}");
      break;
    case SCR3_BLK_ASK_AND_WAIT:
      printf(",\"inputs\":{");
      scr3_emit_input_src("QUESTION", b->input.src);
      printf("},\"fields\":{}");
      break;
    case SCR3_BLK_ANSWER_REPORTER:
      printf(",\"inputs\":{},\"fields\":{}");
      break;
    case SCR3_BLK_STOPALL:
      printf(
          ",\"inputs\":{},\"fields\":{\"STOP_OPTION\":[\"all\",null]},"
          "\"mutation\":{\"tagName\":\"mutation\",\"children\":[],\"hasnext\":"
          "\"true\"}");
      break;
    case SCR3_BLK_DUMMY:
      error("dummy block");
      break;
  }
  printf("}");
}

static void scr3_emit_all(void) {
  for (struct scr3_block *b = scr3_last_block_p; b; b = b->list_next) {
    if (b != scr3_last_block_p) putchar(',');
    scr3_emit_block(b);
  }
}

// -----------------------------------------------------------------------------
// Runtime implementation
// -----------------------------------------------------------------------------

static void scr3_impl_ascii_encode(void) {
  struct scr3_block *p, *q, *r, *substack1, *substack2, *substack3;

  scr3_reg_t reg_i = scr3_add_new_register("ascii_encode:i");

  p = scr3_build_define("ascii_encode %s", "char");

  q = substack1 = scr3_build_set_var(NULL, scr3_reg_impl_ret, scr3_src_imm(""));
  q = scr3_build_set_var(q, reg_i, scr3_src_imm("3"));

  r = substack3 = scr3_build_set_var(
      NULL, scr3_reg_impl_ret,
      scr3_build_join(scr3_src_reg(scr3_reg_impl_ret),
                      scr3_build_letter_of(scr3_src_reg(reg_i),
                                           scr3_build_arg_reporter("char"))));
  r = scr3_build_change_var(r, reg_i, scr3_src_imm("1"));

  q = scr3_build_repeat(q, scr3_src_imm("3"), substack3);

  q = substack2 =
      scr3_build_switch_backdrop(NULL, scr3_build_arg_reporter("char"));
  q = scr3_build_set_var(
      q, scr3_reg_impl_ret,
      scr3_build_sub(scr3_build_backdrop_reporter(false), scr3_src_imm("1")));
  q = scr3_build_switch_backdrop(q, scr3_src_imm("backdrop"));

  p = scr3_build_if_else(
      p,
      scr3_build_equal(scr3_build_length(scr3_build_arg_reporter("char")),
                       scr3_src_imm("5")),
      substack1, substack2);
}
static void scr3_impl_ascii_decode(void) {
  struct scr3_block *p;
  p = scr3_build_define("ascii_decode %s", "code");

  p = scr3_build_switch_backdrop(
      p, scr3_build_add(scr3_build_arg_reporter("code"), scr3_src_imm("1")));
  p = scr3_build_set_var(p, scr3_reg_impl_ret,
                         scr3_build_backdrop_reporter(true));
  p = scr3_build_switch_backdrop(p, scr3_src_imm("backdrop"));
}

static void scr3_impl_base64_encode(void) {
  struct scr3_block *p;
  p = scr3_build_define("base64_encode %s", "num");

  p = scr3_build_switch_backdrop(
      p, scr3_build_add(scr3_build_arg_reporter("num"), scr3_src_imm("129")));
  p = scr3_build_set_var(
      p, scr3_reg_impl_ret,
      scr3_build_letter_of(scr3_src_imm("2"),
                           scr3_build_backdrop_reporter(true)));
  p = scr3_build_switch_backdrop(p, scr3_src_imm("backdrop"));
}
static void scr3_impl_base64_decode(void) {
  struct scr3_block *p;
  p = scr3_build_define("base64_decode %s", "char");

  p = scr3_build_switch_backdrop(
      p, scr3_build_join(scr3_src_imm("!"), scr3_build_arg_reporter("char")));
  p = scr3_build_set_var(
      p, scr3_reg_impl_ret,
      scr3_build_sub(scr3_build_backdrop_reporter(false), scr3_src_imm("129")));
  p = scr3_build_switch_backdrop(p, scr3_src_imm("backdrop"));
}

static void scr3_impl_writeback_page(void) {
  struct scr3_block *p, *q, *substack1, *substack2;

  scr3_reg_t reg_i = scr3_add_new_register("wb:i");
  scr3_reg_t reg_base64 = scr3_add_new_register("wb:x");
  scr3_reg_t reg_tmp = scr3_add_new_register("wb:t");

  p = scr3_build_define("writeback_page", NULL);

  p = scr3_build_set_var(p, reg_i, scr3_src_imm("4096"));
  p = scr3_build_set_var(p, reg_base64, scr3_src_imm(""));

  q = substack2 = scr3_build_call(
      NULL, "base64_encode %s",
      scr3_build_mod(scr3_src_reg(reg_tmp), scr3_src_imm("64")));
  q = scr3_build_set_var(q, reg_base64,
                         scr3_build_join(scr3_src_reg(scr3_reg_impl_ret),
                                         scr3_src_reg(reg_base64)));
  q = scr3_build_set_var(
      q, reg_tmp,
      scr3_build_mathop(
          scr3_build_div(scr3_src_reg(reg_tmp), scr3_src_imm("64")), "floor"));

  q = substack1 = scr3_build_set_var(
      NULL, reg_tmp,
      scr3_build_item_of_list(scr3_src_reg(reg_i), SCR3_LIST_PAGE));
  q = scr3_build_repeat(q, scr3_src_imm("4"), substack2);
  q = scr3_build_change_var(q, reg_i, scr3_src_imm("-1"));

  p = scr3_build_repeat(p, scr3_src_imm("4096"), substack1);
  p = scr3_build_replace_item(p, scr3_src_reg(scr3_reg_impl_page),
                              scr3_src_reg(reg_base64), SCR3_LIST_MEMORY);
}

static void scr3_impl_expand_page(void) {
  struct scr3_block *p, *q, *substack1, *substack2, *substack3, *substack4;

  scr3_reg_t reg_base64str = scr3_add_new_register("ex:s");
  scr3_reg_t reg_i = scr3_add_new_register("ex:i");
  scr3_reg_t reg_x = scr3_add_new_register("ex:x");

  p = scr3_build_define("expand_page", NULL);

  p = scr3_build_delete_all_of_list(p, SCR3_LIST_PAGE);
  p = scr3_build_set_var(
      p, reg_base64str,
      scr3_build_item_of_list(scr3_src_reg(scr3_reg_impl_page),
                              SCR3_LIST_MEMORY));

  substack1 = scr3_build_repeat(
      NULL, scr3_src_imm("4096"),
      scr3_build_add_to_list(NULL, scr3_src_imm("0"), SCR3_LIST_PAGE));

  q = substack4 = scr3_build_call(
      NULL, "base64_decode %s",
      scr3_build_letter_of(scr3_src_reg(reg_i), scr3_src_reg(reg_base64str)));
  q = scr3_build_set_var(
      q, reg_x,
      scr3_build_add(scr3_build_mul(scr3_src_reg(reg_x), scr3_src_imm("64")),
                     scr3_src_reg(scr3_reg_impl_ret)));
  q = scr3_build_change_var(q, reg_i, scr3_src_imm("1"));

  q = substack3 = scr3_build_set_var(NULL, reg_x, scr3_src_imm("0"));
  q = scr3_build_repeat(q, scr3_src_imm("4"), substack4);
  q = scr3_build_add_to_list(q, scr3_src_reg(reg_x), SCR3_LIST_PAGE);

  q = substack2 = scr3_build_set_var(p, reg_i, scr3_src_imm("1"));
  q = scr3_build_repeat(q, scr3_src_imm("4096"), substack3);

  p = scr3_build_if_else(
      p,
      scr3_build_equal(scr3_build_length(scr3_src_reg(reg_base64str)),
                       scr3_src_imm("0")),
      substack1, substack2);
}

static void scr3_impl_store(void) {
  struct scr3_block *p, *substack, *q;

  scr3_reg_t reg_dst_page = scr3_add_new_register("store:x");
  scr3_reg_t reg_value = scr3_add_new_register("store:y");

  p = scr3_build_define("store %s", "dst");

  p = scr3_build_set_var(
      p, reg_dst_page,
      scr3_build_add(
          scr3_build_mathop(scr3_build_div(scr3_build_arg_reporter("dst"),
                                           scr3_src_imm("4096")),
                            "floor"),
          scr3_src_imm("1")));
  p = scr3_build_set_var(p, reg_value, scr3_src_reg(scr3_reg_impl_ret));

  q = substack = scr3_build_call(NULL, "writeback_page", EMPTY_SRC);
  q = scr3_build_set_var(q, scr3_reg_impl_page, scr3_src_reg(reg_dst_page));
  q = scr3_build_call(q, "expand_page", EMPTY_SRC);

  p = scr3_build_if(
      p,
      scr3_build_not(scr3_build_equal(scr3_src_reg(scr3_reg_impl_page),
                                      scr3_src_reg(reg_dst_page))),
      substack);
  p = scr3_build_replace_item(
      p,
      scr3_build_add(
          scr3_build_mod(scr3_build_arg_reporter("dst"), scr3_src_imm("4096")),
          scr3_src_imm("1")),
      scr3_src_reg(reg_value), SCR3_LIST_PAGE);
}

static void scr3_impl_load(void) {
  struct scr3_block *p, *q, *substack1, *substack2, *substack3, *substack4;

  scr3_reg_t reg_x = scr3_add_new_register("load:x");
  scr3_reg_t reg_y = scr3_add_new_register("load:y");
  scr3_reg_t reg_z = scr3_add_new_register("load:z");

  p = scr3_build_define("load %s", "src");

  p = scr3_build_set_var(
      p, reg_x,
      scr3_build_add(
          scr3_build_mathop(scr3_build_div(scr3_build_arg_reporter("src"),
                                           scr3_src_imm("4096")),
                            "floor"),
          scr3_src_imm("1")));
  p = scr3_build_set_var(
      p, reg_z,
      scr3_build_mod(scr3_build_arg_reporter("src"), scr3_src_imm("4096")));

  substack1 = scr3_build_set_var(
      NULL, scr3_reg_impl_ret,
      scr3_build_item_of_list(
          scr3_build_add(scr3_src_reg(reg_z), scr3_src_imm("1")),
          SCR3_LIST_PAGE));

  q = substack4 = scr3_build_call(
      NULL, "base64_decode %s",
      scr3_build_letter_of(scr3_src_reg(reg_z), scr3_src_reg(reg_y)));
  q = scr3_build_set_var(
      q, reg_x,
      scr3_build_add(scr3_build_mul(scr3_src_reg(reg_x), scr3_src_imm("64")),
                     scr3_src_reg(scr3_reg_impl_ret)));
  q = scr3_build_change_var(q, reg_z, scr3_src_imm("1"));

  q = substack3 = scr3_build_set_var(
      NULL, reg_z,
      scr3_build_add(scr3_build_mul(scr3_src_reg(reg_z), scr3_src_imm("4")),
                     scr3_src_imm("1")));
  q = scr3_build_repeat(q, scr3_src_imm("4"), substack4);

  q = substack2 = scr3_build_set_var(
      NULL, reg_y,
      scr3_build_item_of_list(scr3_src_reg(reg_x), SCR3_LIST_MEMORY));
  q = scr3_build_set_var(q, reg_x, scr3_src_imm("0"));
  q = scr3_build_if(
      q,
      scr3_build_gt(scr3_build_length(scr3_src_reg(reg_y)), scr3_src_imm("0")),
      substack3);
  q = scr3_build_set_var(q, scr3_reg_impl_ret, scr3_src_reg(reg_x));

  p = scr3_build_if_else(
      p,
      scr3_build_equal(scr3_src_reg(scr3_reg_impl_page), scr3_src_reg(reg_x)),
      substack1, substack2);
}

static void scr3_impl_getchar(void) {
  struct scr3_block *p, *q, *substack;
  p = scr3_build_define("getchar", NULL);

  q = substack =
      scr3_build_set_var(NULL, scr3_reg_impl_wait, scr3_src_imm("1"));
  q = scr3_build_wait_until(
      q, scr3_build_gt(scr3_build_length_of_list(SCR3_LIST_STDIN),
                       scr3_src_imm("0")));

  p = scr3_build_if(p,
                    scr3_build_equal(scr3_build_length_of_list(SCR3_LIST_STDIN),
                                     scr3_src_imm("0")),
                    substack);
  p = scr3_build_call(
      p, "ascii_encode %s",
      scr3_build_item_of_list(scr3_src_imm("1"), SCR3_LIST_STDIN));
  p = scr3_build_delete_item(p, scr3_src_imm("1"), SCR3_LIST_STDIN);
}

static void scr3_impl_putchar(void) {
  struct scr3_block *p, *substack1, *substack2, *q;
  p = scr3_build_define("putchar", NULL);

  substack1 = scr3_build_add_to_list(NULL, scr3_src_imm(""), SCR3_LIST_STDOUT);
  q = substack2 = scr3_build_call(
      NULL, "ascii_decode %s",
      scr3_build_mod(scr3_src_reg(scr3_reg_impl_ret), scr3_src_imm("256")));
  q = scr3_build_replace_item(
      q, scr3_build_length_of_list(SCR3_LIST_STDOUT),
      scr3_build_join(
          scr3_build_item_of_list(scr3_build_length_of_list(SCR3_LIST_STDOUT),
                                  SCR3_LIST_STDOUT),
          scr3_src_reg(scr3_reg_impl_ret)),
      SCR3_LIST_STDOUT);
  p = scr3_build_if_else(
      p, scr3_build_equal(scr3_src_reg(scr3_reg_impl_ret), scr3_src_imm("10")),
      substack1, substack2);
}

static void scr3_impl_read_stdin(void) {
  struct scr3_block *p, *substack1, *q, *substack2, *r, *substack3, *s;

  scr3_reg_t reg_i = scr3_add_new_register("read:i");
  scr3_reg_t reg_r = scr3_add_new_register("read:r");
  scr3_reg_t reg_t = scr3_add_new_register("read:t");

  p = scr3_build_when_flag_clicked();

  q = substack1 = scr3_build_wait_until(
      NULL,
      scr3_build_equal(scr3_src_reg(scr3_reg_impl_wait), scr3_src_imm("1")));
  q = scr3_build_ask_and_wait(q, scr3_src_imm("stdin"));
  q = scr3_build_set_var(q, reg_i, scr3_src_imm("1"));

  r = substack2 = scr3_build_set_var(NULL, reg_r, scr3_src_imm("1"));
  r = scr3_build_set_var(r, reg_t, scr3_src_imm(""));

  s = substack3 = scr3_build_set_var(
      NULL, reg_t,
      scr3_build_join(scr3_src_reg(reg_t),
                      scr3_build_letter_of(scr3_src_reg(reg_i),
                                           scr3_build_answer_reporter())));
  s = scr3_build_change_var(s, reg_i, scr3_src_imm("1"));
  s = scr3_build_if_else(
      s, scr3_build_equal(scr3_src_reg(reg_t), scr3_src_imm("＼")),
      scr3_build_change_var(NULL, reg_r, scr3_src_imm("1")),
      scr3_build_if(NULL,
                    scr3_build_equal(scr3_src_reg(reg_t), scr3_src_imm("＼d")),
                    scr3_build_change_var(NULL, reg_r, scr3_src_imm("3"))));
  s = scr3_build_change_var(s, reg_r, scr3_src_imm("-1"));

  r = scr3_build_repeat_until(
      r, scr3_build_equal(scr3_src_reg(reg_r), scr3_src_imm("0")), substack3);
  r = scr3_build_add_to_list(r, scr3_src_reg(reg_t), SCR3_LIST_STDIN);
  r = scr3_build_set_var(r, scr3_reg_impl_wait, scr3_src_imm("0"));

  q = scr3_build_repeat_until(
      q,
      scr3_build_gt(scr3_src_reg(reg_i),
                    scr3_build_length(scr3_build_answer_reporter())),
      substack2);
  p = scr3_build_forever(p, substack1);
}

static void scr3_impl_initialize(void) {
  struct scr3_block *p, *q, *substack;
  p = scr3_build_define("initialize", NULL);

  for (scr3_reg_t reg = 0; reg < scr3_reg_count; reg++) {
    p = scr3_build_set_var(p, reg, scr3_src_imm("0"));
  }
  p = scr3_build_delete_all_of_list(p, SCR3_LIST_STDIN);
  p = scr3_build_delete_all_of_list(p, SCR3_LIST_STDOUT);
  p = scr3_build_delete_all_of_list(p, SCR3_LIST_MEMORY);
  p = scr3_build_delete_all_of_list(p, SCR3_LIST_PAGE);
  p = scr3_build_add_to_list(p, scr3_src_imm(""), SCR3_LIST_STDOUT);
  p = scr3_build_set_var(p, scr3_reg_impl_page, scr3_src_imm("1"));
  q = substack =
      scr3_build_add_to_list(NULL, scr3_src_imm(""), SCR3_LIST_MEMORY);
  q = scr3_build_add_to_list(q, scr3_src_imm("0"), SCR3_LIST_PAGE);
  p = scr3_build_repeat(p, scr3_src_imm("4096"), substack);

  p = scr3_build_call(p, "init_memory", EMPTY_SRC);
}

// -----------------------------------------------------------------------------
// Control flow
// -----------------------------------------------------------------------------

static struct scr3_src src_convert(Value value) {
  switch (value.type) {
    case REG:
      return (struct scr3_src){.type = SCR3_SRC_REGISTER, .reg = value.reg};
    case IMM:
      return (struct scr3_src){.type = SCR3_SRC_IMMEDIATE,
                               .value = format("%d", value.imm)};
    default:
      error("invalid value");
  }
}

static void scr3_emit_inst(Inst *inst, struct scr3_block **p) {
  switch (inst->op) {
    case MOV:
      *p = scr3_build_set_var(*p, inst->dst.reg, src_convert(inst->src));
      break;
    case ADD:
      *p = scr3_build_set_var(
          *p, inst->dst.reg,
          scr3_build_mod(scr3_build_add(scr3_src_reg(inst->dst.reg),
                                        src_convert(inst->src)),
                         scr3_src_imm("16777216")));
      break;
    case SUB:
      *p = scr3_build_set_var(
          *p, inst->dst.reg,
          scr3_build_mod(scr3_build_sub(scr3_src_reg(inst->dst.reg),
                                        src_convert(inst->src)),
                         scr3_src_imm("16777216")));
      break;
    case LOAD:
      *p = scr3_build_call(*p, "load %s", src_convert(inst->src));
      *p = scr3_build_set_var(*p, inst->dst.reg,
                              scr3_src_reg(scr3_reg_impl_ret));
      break;
    case STORE:
      *p = scr3_build_set_var(*p, scr3_reg_impl_ret,
                              scr3_src_reg(inst->dst.reg));
      *p = scr3_build_call(*p, "store %s", src_convert(inst->src));
      break;
    case PUTC:
      *p = scr3_build_set_var(*p, scr3_reg_impl_ret, src_convert(inst->src));
      *p = scr3_build_call(*p, "putchar", EMPTY_SRC);
      break;
    case GETC:
      // to input EOS, this backend requires user to input '＼0' explicitly
      *p = scr3_build_call(*p, "getchar", EMPTY_SRC);
      *p = scr3_build_set_var(*p, inst->dst.reg,
                              scr3_src_reg(scr3_reg_impl_ret));
      break;
    case EXIT:
      *p = scr3_build_stop_all(*p);
      break;
    case DUMP:
      break;
    case EQ:
      *p = scr3_build_if_else(
          *p,
          scr3_build_equal(scr3_src_reg(inst->dst.reg), src_convert(inst->src)),
          scr3_build_set_var(NULL, inst->dst.reg, scr3_src_imm("1")),
          scr3_build_set_var(NULL, inst->dst.reg, scr3_src_imm("0")));
      break;
    case NE:
      *p = scr3_build_if_else(
          *p,
          scr3_build_equal(scr3_src_reg(inst->dst.reg), src_convert(inst->src)),
          scr3_build_set_var(NULL, inst->dst.reg, scr3_src_imm("0")),
          scr3_build_set_var(NULL, inst->dst.reg, scr3_src_imm("1")));
      break;
    case LT:
      *p = scr3_build_if_else(
          *p,
          scr3_build_lt(scr3_src_reg(inst->dst.reg), src_convert(inst->src)),
          scr3_build_set_var(NULL, inst->dst.reg, scr3_src_imm("1")),
          scr3_build_set_var(NULL, inst->dst.reg, scr3_src_imm("0")));
      break;
    case GT:
      *p = scr3_build_if_else(
          *p,
          scr3_build_gt(scr3_src_reg(inst->dst.reg), src_convert(inst->src)),
          scr3_build_set_var(NULL, inst->dst.reg, scr3_src_imm("1")),
          scr3_build_set_var(NULL, inst->dst.reg, scr3_src_imm("0")));
      break;
    case LE:
      *p = scr3_build_if_else(
          *p,
          scr3_build_gt(scr3_src_reg(inst->dst.reg), src_convert(inst->src)),
          scr3_build_set_var(NULL, inst->dst.reg, scr3_src_imm("0")),
          scr3_build_set_var(NULL, inst->dst.reg, scr3_src_imm("1")));
      break;
    case GE:
      *p = scr3_build_if_else(
          *p,
          scr3_build_lt(scr3_src_reg(inst->dst.reg), src_convert(inst->src)),
          scr3_build_set_var(NULL, inst->dst.reg, scr3_src_imm("0")),
          scr3_build_set_var(NULL, inst->dst.reg, scr3_src_imm("1")));
      break;
    case JEQ:
      *p = scr3_build_if(
          *p,
          scr3_build_equal(scr3_src_reg(inst->dst.reg), src_convert(inst->src)),
          scr3_build_set_var(
              NULL, scr3_reg_pc,
              scr3_build_sub(src_convert(inst->jmp), scr3_src_imm("1"))));
      break;
    case JNE:
      *p = scr3_build_if_else(
          *p,
          scr3_build_equal(scr3_src_reg(inst->dst.reg), src_convert(inst->src)),
          NULL,
          scr3_build_set_var(
              NULL, scr3_reg_pc,
              scr3_build_sub(src_convert(inst->jmp), scr3_src_imm("1"))));
      break;
    case JLT:
      *p = scr3_build_if(
          *p,
          scr3_build_lt(scr3_src_reg(inst->dst.reg), src_convert(inst->src)),
          scr3_build_set_var(
              NULL, scr3_reg_pc,
              scr3_build_sub(src_convert(inst->jmp), scr3_src_imm("1"))));
      break;
    case JGT:
      *p = scr3_build_if(
          *p,
          scr3_build_gt(scr3_src_reg(inst->dst.reg), src_convert(inst->src)),
          scr3_build_set_var(
              NULL, scr3_reg_pc,
              scr3_build_sub(src_convert(inst->jmp), scr3_src_imm("1"))));
      break;
    case JLE:
      *p = scr3_build_if_else(
          *p,
          scr3_build_gt(scr3_src_reg(inst->dst.reg), src_convert(inst->src)),
          NULL,
          scr3_build_set_var(
              NULL, scr3_reg_pc,
              scr3_build_sub(src_convert(inst->jmp), scr3_src_imm("1"))));
      break;
    case JGE:
      *p = scr3_build_if_else(
          *p,
          scr3_build_lt(scr3_src_reg(inst->dst.reg), src_convert(inst->src)),
          NULL,
          scr3_build_set_var(
              NULL, scr3_reg_pc,
              scr3_build_sub(src_convert(inst->jmp), scr3_src_imm("1"))));
      break;
    case JMP:
      scr3_build_set_var(
          *p, scr3_reg_pc,
          scr3_build_sub(src_convert(inst->jmp), scr3_src_imm("1")));
      break;

    default:
      error("oops");
  }
}

// generate binary search [lval, hval)
static struct scr3_block *scr3_impl_switch_helper(scr3_reg_t reg, int lval,
                                                  int hval,
                                                  struct scr3_block *cases[],
                                                  int cases_size) {
  if (hval - lval <= 1) {
    return lval < cases_size ? cases[lval] : NULL;
  } else {
    int mval = (lval + hval) / 2;
    struct scr3_block *case1, *case2;
    case1 = scr3_impl_switch_helper(reg, lval, mval, cases, cases_size);
    case2 = scr3_impl_switch_helper(reg, mval, hval, cases, cases_size);
    if (!case1 && !case2) return NULL;

    struct scr3_block *if_else = scr3_build_if_else(
        NULL,
        scr3_build_lt(scr3_src_reg(reg), scr3_src_imm(format("%d", mval))),
        case1, case2);
    if (case1) case1->prev = if_else;
    if (case2) case2->prev = if_else;
    return if_else;
  }
}

static struct scr3_block *scr3_impl_chunk(int low_pc, int high_pc,
                                          struct scr3_block *each_pc[],
                                          int max_pc) {
  struct scr3_block *sw =
      scr3_impl_switch_helper(scr3_reg_pc, low_pc, high_pc, each_pc, max_pc);
  if (!sw) return NULL;
  scr3_build_change_var(sw, scr3_reg_pc, scr3_src_imm("1"));

  struct scr3_block *p, *def;
  p = def = scr3_build_define(format("pc%d-%d", low_pc, high_pc), NULL);

  p = scr3_build_repeat_until(
      p,
      scr3_build_or(scr3_build_lt(scr3_src_reg(scr3_reg_pc),
                                  scr3_src_imm(format("%d", low_pc))),
                    scr3_build_lt(scr3_src_imm(format("%d", high_pc - 1)),
                                  scr3_src_reg(scr3_reg_pc))),
      sw);
  return def;
}

static const int SCR3_CHUNK_SIZE = 512;
static struct scr3_block *scr3_impl_chunk_switch(struct scr3_block *parent,
                                                 struct scr3_block *each_pc[],
                                                 int each_pc_size) {
  scr3_reg_t reg_chunk = scr3_add_new_register("chunk:x");

  int switch_size = 1;
  while (switch_size < each_pc_size) switch_size *= 2;
  int chunk_count = (switch_size + SCR3_CHUNK_SIZE - 1) / SCR3_CHUNK_SIZE;
  struct scr3_block **cases = malloc(sizeof(struct scr3_block *) * chunk_count);
  for (int i = 0; i < chunk_count; i++) {
    if (!scr3_impl_chunk(i * SCR3_CHUNK_SIZE, (i + 1) * SCR3_CHUNK_SIZE,
                         each_pc, each_pc_size)) {
      continue;
    }
    cases[i] = scr3_build_call(
        NULL, format("pc%d-%d", i * SCR3_CHUNK_SIZE, (i + 1) * SCR3_CHUNK_SIZE),
        EMPTY_SRC);
  }

  struct scr3_block *q, *sw;
  sw = scr3_build_set_var(
      NULL, reg_chunk,
      scr3_build_mathop(
          scr3_build_div(scr3_src_reg(scr3_reg_pc),
                         scr3_src_imm(format("%d", SCR3_CHUNK_SIZE))),
          "floor"));
  q = scr3_impl_switch_helper(reg_chunk, 0, chunk_count, cases, chunk_count);
  if (q) q->prev = sw;
  sw->next = q;

  return scr3_build_forever(parent, sw);
}

static void scr3_impl_main_loop(Inst *inst) {
  struct scr3_block *p, *q = NULL;
  p = scr3_create_block(SCR3_BLK_WHEN_FLAG_CLICKED, NULL, NULL);
  p = scr3_build_call(p, "initialize", EMPTY_SRC);

  int cases_size = 0;
  for (Inst *itr = inst; itr; itr = itr->next)
    cases_size = max(cases_size, itr->pc);
  cases_size += 1;

  struct scr3_block **cases = malloc(sizeof(struct scr3_block *) * cases_size);
  for (int i = 0; i < cases_size; i++) cases[i] = NULL;

  int prev_pc = -1;
  for (; inst; inst = inst->next) {
    if (prev_pc != inst->pc) {
      q = cases[inst->pc] = scr3_create_block(SCR3_BLK_DUMMY, NULL, NULL);
    }
    prev_pc = inst->pc;
    scr3_emit_inst(inst, &q);
  }
  p = scr3_impl_chunk_switch(p, cases, cases_size);
}

static void scr3_impl_init_memory(Data *data) {
  struct scr3_block *p = scr3_build_define("init_memory", NULL);
  for (int mp = 0; data; data = data->next, mp++) {
    if (data->v) {
      p = scr3_build_set_var(p, scr3_reg_impl_ret,
                             scr3_src_imm(format("%d", data->v)));
      p = scr3_build_call(p, "store %s", scr3_src_imm(format("%d", mp)));
    }
  }
}

void target_scratch3(Module *module) {
  scr3_reg_a = scr3_add_new_register("a");
  scr3_reg_b = scr3_add_new_register("b");
  scr3_reg_c = scr3_add_new_register("c");
  scr3_reg_d = scr3_add_new_register("d");
  scr3_reg_sp = scr3_add_new_register("sp");
  scr3_reg_bp = scr3_add_new_register("bp");
  scr3_reg_pc = scr3_add_new_register("pc");
  scr3_reg_impl_ret = scr3_add_new_register("_r");
  scr3_reg_impl_page = scr3_add_new_register("_p");
  scr3_reg_impl_wait = scr3_add_new_register("_w");

  scr3_impl_read_stdin();
  scr3_impl_ascii_encode();
  scr3_impl_ascii_decode();
  scr3_impl_base64_decode();
  scr3_impl_base64_encode();
  scr3_impl_writeback_page();
  scr3_impl_expand_page();
  scr3_impl_store();
  scr3_impl_load();
  scr3_impl_initialize();
  scr3_impl_getchar();
  scr3_impl_putchar();
  scr3_impl_init_memory(module->data);
  scr3_impl_main_loop(module->text);

  // ---------------------------------------------------------------------------

  printf("{\"targets\":[{\"isStage\":true,\"name\":\"Stage\",");
  printf("\"variables\":{");
  for (scr3_reg_t reg = 0; reg < scr3_reg_count; reg++) {
    if (reg) putchar(',');
    const char *name = scr3_register_name[reg];
    printf("\"#r:%s\":[\"%s\", \"0\"]", name, name);
  }
  printf("},\"lists\":{");
  for (int list = 0; list < SCR3_NUM_OF_LIST; list++) {
    if (list) putchar(',');
    const char *name = SCR3_LIST_NAME[list];
    printf("\"#l:%s\":[\"%s\", []]", name, name);
  }
  printf("},\"broadcasts\":{},\"blocks\":{");

  scr3_emit_all();

  printf("},\"comments\":{},\"currentCostume\":192,\"costumes\":[");

  // embed ASCII table in backdrops
  for (int i = 0; i < 128; i++) {
    char escaped_char_str[10];
    if (32 <= i && i < 127) {
      sprintf(escaped_char_str, (i == '"' || i == '\\') ? "\\%c" : "%c", i);
    } else if (i == 10) {
      sprintf(escaped_char_str, "＼n");
    } else {
      sprintf(escaped_char_str, "＼%d", i);
    }
    printf(
        "{\"name\":\"%s\",\"assetId\":\"%s\",\"md5ext\":\"%s.svg\","
        "\"dataFormat\":\"svg\"},",
        escaped_char_str, SCR3_SVG_MD5, SCR3_SVG_MD5);
  }
  // embed Base64 table in backdrops (with prefix '!')
  for (int i = 0; i < 64; i++) {
    printf(
        "{\"name\":\"!%s\",\"assetId\":\"%s\",\"md5ext\":\"%s.svg\","
        "\"dataFormat\":\"svg\"},",
        SCR3_BASE64_TABLE[i], SCR3_SVG_MD5, SCR3_SVG_MD5);
  }

  // main backdrop
  printf(
      "{\"name\":\"backdrop\",\"assetId\":\"%s\",\"md5ext\":\"%s.svg\","
      "\"dataFormat\":\"svg\"}",
      SCR3_SVG_MD5, SCR3_SVG_MD5);

  printf(
      "],\"sounds\":[],\"volume\":100,\"layerOrder\":0,\"tempo\":60,"
      "\"videoTransparency\":50,\"videoState\":\"on\",\"textToSpeechLanguage\":"
      "null}],\"monitors\":[{\"id\":\"#l:stdout\",\"mode\":\"list\","
      "\"opcode\":");
  printf(
      "\"data_listcontents\",\"params\":{\"LIST\":\"stdout\"},\"spriteName\":"
      "null,\"value\":[],\"width\":480,\"height\":280,\"x\":0,\"y\":0,"
      "\"visible\":true}],\"extensions\":[],\"meta\":{\"semver\":\"3.0.0\"}}");
  return;
}
