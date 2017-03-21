#include <assert.h>
#include <ctype.h>
#include <ir/ir.h>
#include <target/util.h>

int tm_next_state;
int tm_new_state() {
  return tm_next_state++;
}
int tm_q_reject;

/* The tape is laid out like this:
     ^_00000...r_0_..._0_v_0_..._0_..._a_0_..._0_v_0_..._0_..._o000...0_...$
       input   register    value       address   value         output

   The blanks between the symbols are used as scratch space. */

typedef enum {TM_BLANK, TM_START, TM_END, TM_ZERO, TM_ONE, TM_REGISTER, TM_ADDRESS, TM_VALUE, TM_OUTPUT, TM_SRC, TM_DST, TM_NUM_SYMBOLS} tm_symbol_t;
const char *tm_symbol_names[] = {"_", "^", "$", "0", "1", "r", "a", "v", "o", "s", "d"};
const int tm_bit[2] = {TM_ZERO, TM_ONE};

const int tm_word_size = 24;

typedef enum {
  TM_SKIP_BEFORE_SRC = 1,
  TM_SKIP_AFTER_SRC = 2,
  TM_SKIP_BEFORE_DST = 4,
  TM_SKIP_AFTER_DST = 8
} tm_writemode_t;

int tm_intcmp(int x, int y) {
  if (x > y) return +1;
  else if (x < y) return -1;
  else return 0;
}

void tm_comment(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  char* r = vformat(fmt, ap);
  va_end(ap);
  printf("// %s\n", r);
}

/* These functions take a start state and an accept state(s) as
   arguments and, as a convenience, return the accept state. */

/* Generate a transition from state q, read symbol a,
   write symbol b, move in direction d, go to state r. */

int tm_transition(int q, tm_symbol_t a, tm_symbol_t b, int d, int r) {
  const char *dname;
  if (d == -1)      dname = "L";
  else if (d == 0)  dname = "N"; 
  else if (d == +1) dname = "R";
  else error("invalid direction %d", d);
  emit_line("%d %s %d %s %s", 
	    q, tm_symbol_names[a], 
	    r, tm_symbol_names[b], dname);
  return r;
}

/* Generate transitions to write symbol b and move in direction d,
   regardless of input symbol. */

int tm_write(int q, tm_symbol_t b, int d, int r) {
  for (tm_symbol_t a=0; a<TM_NUM_SYMBOLS; a++)
    tm_transition(q, a, b, d, r);
  return r;
}

/* Generate write transitions that do one thing for symbol a, 
   and another thing for all other symbols.

   Returns the state for the latter case. */

int tm_write_if(int q, 
		tm_symbol_t a, int ba, int da, int ra,
		tm_symbol_t b, int d, int r) {
  for (tm_symbol_t s=0; s<TM_NUM_SYMBOLS; s++)
    if (s == a) tm_transition(q, s, ba, da, ra); 
    else        tm_transition(q, s, b, d, r);
  return r;
}

int tm_write_if2(int q, 
		 tm_symbol_t a1, int b1, int d1, int r1,
		 tm_symbol_t a2, int b2, int d2, int r2,
		 tm_symbol_t b, int d, int r) {
  for (tm_symbol_t s=0; s<TM_NUM_SYMBOLS; s++)
    if (s == a1)      tm_transition(q, s, b1, d1, r1); 
    else if (s == a2) tm_transition(q, s, b2, d2, r2); 
    else              tm_transition(q, s, b, d, r);
  return r;
}

/* Generate transitions to move in direction d, regardless of input symbol. */

int tm_move(int q, int d, int r) {
  for (tm_symbol_t s=0; s<TM_NUM_SYMBOLS; s++)
    tm_transition(q, s, s, d, r);
  return r;
}

/* Generate move transitions that do one thing for symbol a, 
   and another thing for all other symbols.

   Returns the state for the latter case. */

int tm_move_if(int q, 
	       tm_symbol_t a, int da, int ra, 
	       int d, int r) {
  for (tm_symbol_t s=0; s<TM_NUM_SYMBOLS; s++)
    if (s == a)      tm_transition(q, s, s, da, ra);
    else             tm_transition(q, s, s, d, r);
  return r;
}

/* Generate move transitions that do one thing for symbol a, 
   another thing for symbol b,
   and another thing for all other symbols.

   Returns the state for the last case. */

int tm_move_if2(int q, 
		tm_symbol_t a, int da, int ra, 
		tm_symbol_t b, int db, int rb, 
		int d, int r) {
  for (tm_symbol_t s=0; s<TM_NUM_SYMBOLS; s++)
    if (s == a)      tm_transition(q, s, s, da, ra);
    else if (s == b) tm_transition(q, s, s, db, rb);
    else             tm_transition(q, s, s, d, r);
  return r;
}

/* Generate transitions that just change state and do nothing else. */

int tm_noop(int q, int r) { 
  return tm_move(q, 0, r); 
}

/* Inserts a symbol and shifts existing tm_bits to the left/right until a
   non-tm_bit is reached. Afterwards, the head is on the first
   non-tm_bit. */

int tm_insert(int q, tm_symbol_t a, int d, int r) {
  int q0 = tm_new_state(), q1 = tm_new_state();
  tm_write_if2(q,  TM_ZERO, a,    d, q0, TM_ONE, a,    d, q1, a,    d, r);
  tm_write_if2(q0, TM_ZERO, TM_ZERO, d, q0, TM_ONE, TM_ZERO, d, q1, TM_ZERO, d, r);
  tm_write_if2(q1, TM_ZERO, TM_ONE,  d, q0, TM_ONE, TM_ONE,  d, q1, TM_ONE,  d, r);
  return r;
}

/* Generate transitions to write a binary number,
   leaving a scratch cell before each tm_bit. */

int tm_write_tm_bits(int q, unsigned int x, int n, int mode, int r) {
  for (int i=n-1; i>=0; i--) {
    if (mode & TM_SKIP_BEFORE_DST)
      q = tm_move(q, +1, tm_new_state());
    q = tm_write(q, (1<<i)&x?TM_ONE:TM_ZERO, +1, tm_new_state());
    if (mode & TM_SKIP_AFTER_DST)
      q = tm_move(q, +1, tm_new_state());
  }
  return tm_noop(q, r);
}

int tm_erase_tm_bits(int q, int r) {
  int erase = q, skip = tm_new_state();
  tm_write_if2(erase, TM_ZERO, TM_BLANK, +1, skip, TM_ONE, TM_BLANK, +1, skip, TM_BLANK, 0, r);
  tm_move(skip, +1, erase);
  return r;
}

/* Generate transitions to search in direction d for symbol a,
   or until the end of the used portion of the tape is reached. */

int tm_find(int q, int d, int a, int r_yes, int r_no) {
  tm_symbol_t marker = d < 0 ? TM_START : TM_END;
  tm_move_if2(q, a, 0, r_yes, marker, 0, r_no, d, q);
  return r_yes;
}

/* Generate transitions to move to the left or right end of the tape. */

int tm_rewind(int q, int r) { 
  tm_move_if(q, TM_START, 0, r, -1, q);
  return r;
}

int tm_ffwd(int q, int r) { 
  tm_move_if(q, TM_END, 0, r, +1, q);
  return r;
}

/* Generate transitions to find register reg or address addr. The head
   ends on the scratch cell to the left of the corresponding value. */

int tm_find_location(int q, tm_symbol_t type, int addr, int width, int r_found, int r_notfound) {
  int qstart = q;
  q = tm_find(q, +1, type, tm_new_state(), r_notfound);   // _[r]_0_1 ... _v_0_1
  q = tm_move(q, +1, tm_new_state());                     // _r[_]0_1 ... _v_0_1
  for (int i=width-1; i>=0; i--) {
    q = tm_move(q, +1, tm_new_state());                   // _r_[0]_1 ... _v_0_1
    tm_symbol_t tm_bit = (1<<i)&addr ? TM_ONE : TM_ZERO;
    int q_match = tm_new_state();
    tm_move_if(q,
	       tm_bit, +1, q_match,                       // _r_0[_]1 ... _v_0_1
	       +1, qstart);
    q = q_match;
  }
  q = tm_move(q, +1, tm_new_state());                     // _r_0_1 ... _[v]_0_1
  tm_move_if(q,
	     TM_VALUE, +1, r_found,                       // _r_0_1 ... _v[_]0_1
	     0, tm_q_reject);
  return r_found;
}

int tm_new_location(int q, tm_symbol_t type, int addr, int awidth, int val, int r) {
  q = tm_ffwd(q, tm_new_state());
  q = tm_write(q, type, +1, tm_new_state());
  q = tm_move(q, +1, tm_new_state());
  q = tm_write_tm_bits(q, addr, awidth, TM_SKIP_AFTER_DST, tm_new_state());
  q = tm_write(q, TM_VALUE, +1, tm_new_state());
  q = tm_move(q, +1, tm_new_state());
  q = tm_write_tm_bits(q, val, tm_word_size, TM_SKIP_AFTER_DST, tm_new_state());
  return tm_write(q, TM_END, -1, r);
}

int tm_find_register(int q, Reg reg, int r) {
  q = tm_rewind(q, tm_new_state()); // to do: can be slow if input is long
  return tm_find_location(q, TM_REGISTER, reg, 3, r, tm_q_reject);
}

int tm_find_memory(int q, int addr, int r) {
  q = tm_rewind(q, tm_new_state()); // to do: can be slow if input is long
  int q_append = tm_new_state();
  tm_find_location(q, TM_ADDRESS, addr, tm_word_size, r, q_append);
  q = tm_new_location(q_append, TM_ADDRESS, addr, tm_word_size, 0, tm_new_state());
  q = tm_find(q, -1, TM_VALUE, tm_new_state(), tm_q_reject);
  tm_move(q, +1, r);
  return r;
}

/* Copy tm_bits from current position to the position marked by TM_DST.

   The head starts on the scratch cell to the left of the source word,
   and ends on the cell to the right of the destination word (which is
   erased). */

int tm_copy(int q, int d, unsigned int mode, int r) {
  // The examples show mode == TM_SKIP_BEFORE_SRC|TM_SKIP_AFTER_DST.
  int q_store[2] = {tm_new_state(), tm_new_state()};
  int q_cleansrc = tm_new_state(), q_cleandst = tm_new_state();
  int q_nexttm_bit = q;                                  

  if (mode & TM_SKIP_BEFORE_SRC) {
    // Write TM_SRC, then store tm_bit
                                                        // [_]0_1 ... d_x
    q = tm_write(q, TM_SRC, 0, tm_new_state());               // [s]0_1 ... d_x
    q = tm_move(q, +1, tm_new_state());                    // s[0]_1 ... d_x
    tm_move_if2(q,
		TM_ZERO, 0, q_store[0],                    // s[0]_1 ... d_x
		TM_ONE,  0, q_store[1],
		0, q_cleansrc);
  } else {
    // Store tm_bit, then write TM_SRC
    tm_write_if2(q, 
		 TM_ZERO, TM_SRC, 0, q_store[0], 
		 TM_ONE, TM_SRC, 0, q_store[1], 
		 TM_BLANK, 0, q_cleandst);
  }
  int q_write = tm_new_state();
  for (int b=0; b<2; b++) {
    q = q_store[b];
    q = tm_find(q, d, TM_DST, tm_new_state(), tm_q_reject);    // s0_1 ... [d]_x
    if (mode & TM_SKIP_BEFORE_DST)
      q = tm_write(q, TM_BLANK, +1, tm_new_state());
    tm_write(q, tm_bit[b], +1, q_write);                 // s0_1 ... 0[_]x
  }
  q = q_write;
  if (mode & TM_SKIP_AFTER_DST)
    q = tm_move(q, +1, tm_new_state());                  // s0_1 ... 0[_]x
  q = tm_write(q, TM_DST, 0, tm_new_state());               // s0_1 ... 0[d]x
  q = tm_find(q, -d, TM_SRC, tm_new_state(), tm_q_reject);     // [s]0_1 ... 0dx
  q = tm_write(q, TM_BLANK, +1, tm_new_state());            // _[0]_1 ... 0dx
  if (mode & (TM_SKIP_BEFORE_SRC|TM_SKIP_AFTER_SRC))
    tm_move(q, +1, q_nexttm_bit);                        // _0[_]1 ... 0dx
  else
    tm_noop(q, q_nexttm_bit);

  q = q_cleansrc;
  q = tm_move(q, -1, tm_new_state());
  q = tm_write(q, TM_BLANK, 0, q_cleandst);
  q = tm_find(q, d, TM_DST, tm_new_state(), tm_q_reject);
  return tm_write(q, TM_BLANK, 0, r);
}

/* Gets value from inst->src and copies it to inst->dst with mode
   indicated by mode. */

int tm_copy_value(int q, Inst *inst, int mode, int r) {
  assert(inst->dst.type == REG);
  q = tm_find_register(q, inst->dst.reg, tm_new_state()); // v[_]x_x...
  if (inst->src.type == REG) {
    if (inst->dst.reg == inst->src.reg) {
      if (mode == TM_SKIP_AFTER_DST) {
	// Copy each tm_bit to the cell to its left
	int q_end = tm_new_state(), q_next = q;
	q = tm_move(q, +1, tm_new_state());
	tm_move_if2(q, TM_ZERO, +1, q_next, TM_ONE, +1, q_next, -1, q_end);
	q = q_end;
	q = tm_move(q, -1, tm_new_state());
	q_next = q;
	int q0 = tm_new_state(), q1 = tm_new_state();
	q_end = tm_new_state();
	tm_move_if2(q, TM_ZERO, -1, q0, TM_ONE, -1, q1, +1, q_end);
	tm_write(q0, TM_ZERO, -1, q_next);
	tm_write(q1, TM_ONE,  -1, q_next);
	q = q_end;
	tm_move_if2(q, TM_ZERO, +1, q, TM_ONE, +1, q, 0, r);
      } else {
	// Do nothing
	tm_noop(q, r);
      }
    } else {
      q = tm_write(q, TM_DST, -1, tm_new_state());
      q = tm_find_register(q, inst->src.reg, tm_new_state());
      q = tm_copy(q, tm_intcmp(inst->dst.reg, inst->src.reg), TM_SKIP_BEFORE_SRC|mode, r);
    }
  } else if (inst->src.type == IMM) {
    q = tm_write_tm_bits(q, inst->src.imm, tm_word_size, mode, r);
  } else
    error("invalid src type");
  return r;
}

/* Add/subtract binary number in scratch cells to/from binary number
   in main cells, while erasing the scratch cells. */

int tm_addsub(int q, int c, int r) {
  assert (c == -1 || c == +1);

  // Go to end of numbers
  int q_end = tm_new_state();
  q = tm_move_if2(q, TM_ZERO, +1, q, TM_ONE, +1, q, -1, q_end);

  // Perform the operation, putting result in scratch cells

  // Carry tm_bit
  int carry[2] = {q, tm_new_state()};
  if (c == -1) { int tmp = carry[0]; carry[0] = carry[1]; carry[1] = tmp; }

  // Carry tm_bit plus tm_bit from first number
  int inter[3] = {tm_new_state(), tm_new_state(), tm_new_state()};

  int q_shift = tm_new_state();
  for (int a=0; a<2; a++) {
    tm_transition(carry[a], TM_ZERO,  TM_BLANK, -1, inter[a]);
    tm_transition(carry[a], TM_ONE,   TM_BLANK, -1, inter[a+1]);
    tm_transition(carry[a], TM_VALUE, TM_VALUE, +1, q_shift);
  }
  for (int a=0; a<3; a++)
    for (int b=0; b<2; b++) {
      tm_transition(inter[a], tm_bit[c == +1 ? b : !b], tm_bit[(a+b)&1], -1, carry[(a+b)>>1]);
    }
  c = 0;

  // Shift scratch cells to main cells
  int q0 = tm_new_state(), q1 = tm_new_state();
  q = q_shift;
  tm_write_if2(q, TM_ZERO, TM_BLANK, +1, q0, TM_ONE, TM_BLANK, +1, q1, TM_BLANK, 0, r);
  q0 = tm_write(q0, TM_ZERO, +1, q);
  q1 = tm_write(q1, TM_ONE,  +1, q);
  return r;
}

/* Compare binary number in scratch cells to binary number in main
   cells, while erasing the scratch cells. */

int tm_compare(int q, Op op, int r_true, int r_false) {
  int q_lt, q_eq, q_gt;
  switch (op) {
  case JEQ: case EQ: q_lt = r_false; q_eq = r_true;  q_gt = r_false; break;
  case JNE: case NE: q_lt = r_true;  q_eq = r_false; q_gt = r_true;  break;
  case JLT: case LT: q_lt = r_true;  q_eq = r_false; q_gt = r_false; break;
  case JGE: case GE: q_lt = r_false; q_eq = r_true;  q_gt = r_true;  break;
  case JGT: case GT: q_lt = r_false; q_eq = r_false; q_gt = r_true;  break;
  case JLE: case LE: q_lt = r_true;  q_eq = r_true;  q_gt = r_false; break;
  default: error("invalid comparison operation");
  }

  int q_nexttm_bit = q;
  int q0 = tm_new_state(), q1 = tm_new_state();
  int erase_gt = tm_new_state(), erase_lt = tm_new_state();
  tm_write_if2(q, TM_ZERO, TM_BLANK, +1, q0, TM_ONE, TM_BLANK, +1, q1, TM_BLANK, 0, q_eq);
  tm_move_if2(q0, TM_ZERO, +1, q_nexttm_bit, TM_ONE, +1, erase_gt, 0, tm_q_reject);
  tm_move_if2(q1, TM_ZERO, +1, erase_lt, TM_ONE, +1, q_nexttm_bit, 0, tm_q_reject);
  tm_erase_tm_bits(erase_lt, q_lt);
  tm_erase_tm_bits(erase_gt, q_gt);
  return r_true;
}

/* Test whether main cells are equal to scratch cells, without erasing
   scratch cells. */

int tm_equal(int q, int r_eq, int r_ne) {
  int q_nexttm_bit = q;
  int q0 = tm_new_state(), q1 = tm_new_state();
  tm_move_if2(q, TM_ZERO, +1, q0, TM_ONE, +1, q1, 0, r_eq);
  tm_move_if2(q0, TM_ZERO, +1, q_nexttm_bit, TM_ONE, +1, r_ne, 0, tm_q_reject);
  tm_move_if2(q1, TM_ZERO, +1, r_ne, TM_ONE, +1, q_nexttm_bit, 0, tm_q_reject);
  return r_eq;
}

/* Reads address from current location on tape, and finds the memory
   location with that address, creating a new memory location if
   necessary. */

int tm_find_memory_indirect(int q, int reg, int r) {
  // mark first address as TM_DST
  int q_append = tm_new_state();
  q = tm_find(q, +1, TM_ADDRESS, tm_new_state(), q_append);
  q = tm_move(q, +1, tm_new_state());
  q = tm_write(q, TM_DST, -1, tm_new_state());
  q = tm_find_register(q, reg, tm_new_state());

  q = tm_copy(q, +1, TM_SKIP_BEFORE_SRC|TM_SKIP_AFTER_DST, tm_new_state()); // _axyxyxy...[_]v_z_z_z...

  // compare addresses
  int q_nextaddr = tm_new_state();
  int q_compare = q;
  q = tm_find(q, -1, TM_ADDRESS, tm_new_state(), tm_q_reject);
  q = tm_move(q, +1, tm_new_state());
  q = tm_equal(q, tm_new_state(), q_nextaddr);

  // if equal: move right to value
  q = tm_move(q, +1, tm_new_state());
  tm_move(q, +1, r);

  // if not equal: 
  // mark next address as TM_DST
  q = q_nextaddr;
  q = tm_find(q, +1, TM_ADDRESS, tm_new_state(), q_append);
  q = tm_move(q, +1, tm_new_state());
  q = tm_write(q, TM_DST, -1, tm_new_state());

  // return to previous address
  q = tm_move(q, -1, tm_new_state());
  q = tm_move(q, -1, tm_new_state());
  q = tm_find(q, -1, TM_ADDRESS, tm_new_state(), tm_q_reject);
  q = tm_move(q, +1, tm_new_state());
  
  // move address
  tm_copy(q, +1, TM_SKIP_AFTER_SRC|TM_SKIP_AFTER_DST, q_compare);

  // if no more addresses: create new memory location
  q = q_append;
  q = tm_write(q, TM_ADDRESS, +1, tm_new_state());
  q = tm_write(q, TM_DST, -1, tm_new_state());
  q = tm_move(q, -1, tm_new_state());
  q = tm_find(q, -1, TM_ADDRESS, tm_new_state(), tm_q_reject);
  q = tm_move(q, +1, tm_new_state());
  q = tm_copy(q, +1, TM_SKIP_AFTER_SRC|TM_SKIP_BEFORE_DST, tm_new_state());
  q = tm_move(q, +1, tm_new_state());
  q = tm_write(q, TM_VALUE, +1, tm_new_state());
  q = tm_write_tm_bits(q, 0, tm_word_size, TM_SKIP_BEFORE_DST, tm_new_state());
  q = tm_move(q, +1, tm_new_state());
  q = tm_write(q, TM_END, -1, tm_new_state());
  q = tm_find(q, -1, TM_VALUE, tm_new_state(), tm_q_reject);
  tm_move(q, +1, r);
  return r;
}

/* Create the trie node corresponding to the first (tm_word_size-h) tm_bits of i.
   Returns the trie node.
 */

int tm_make_jmpreg(int pc_max, int h, int i) {
  if (h == 0) {
    return i;
  } else {
    int q_new = tm_new_state();
    int q = tm_transition(q_new, TM_BLANK, TM_BLANK, +1, tm_new_state());
    int q0 = tm_make_jmpreg(pc_max, h-1, i);
    tm_transition(q, TM_ZERO, TM_ZERO, +1, q0);
    if (i+(1<<(h-1)) <= pc_max) {
      int q1 = tm_make_jmpreg(pc_max, h-1, i + (1<<(h-1)));
      tm_transition(q, TM_ONE, TM_ONE, +1, q1);
    }
    return q_new;
  }
}

void target_tm(Module* module) {
  /* Every basic block's entry point is the state with the same number
     as its pc. Additional states are numbered starting after the
     highest pc. */
  int pc_max = 0;
  for (Inst* inst = module->text; inst; inst = inst->next)
    if (inst->pc >= pc_max)
      pc_max = inst->pc;
  tm_next_state = pc_max+1;
  tm_q_reject = tm_new_state();

  tm_comment("trie for jmp reg");
  int q_jmpreg = tm_make_jmpreg(pc_max, tm_word_size, 0);

  int q = 0; // start state (pc 0)

  tm_comment("beginning-of-tape and input string");

  // Insert TM_START and a single scratch cell before input string
  q = tm_insert(q, TM_START, +1, tm_new_state());
  q = tm_rewind(q, tm_new_state());
  q = tm_move(q, +1, tm_new_state());
  q = tm_insert(q, TM_BLANK, +1, tm_new_state());
  q = tm_write(q, TM_END, 0, tm_new_state());

  // Initialize registers
  for (Reg reg=0; reg<6; reg++) {
    tm_comment("register %s value 0", reg_names[reg]);
    q = tm_new_location(q, TM_REGISTER, reg, 3, 0, tm_new_state());
  }

  // Initialize memory
  Data *data = module->data;
  int mp = 0;
  while (data) {
    if (0 <= data->v && data->v < 128 && isprint(data->v))
      tm_comment("address %d value %d '%c'", mp, data->v, data->v);
    else
      tm_comment("address %d value %d", mp, data->v);
    q = tm_new_location(q, TM_ADDRESS, mp, tm_word_size, data->v, tm_new_state());
    data = data->next;
    mp++;
  }
  q = tm_write(q, TM_BLANK, +1, tm_new_state());
  q = tm_write(q, TM_END, -1, tm_new_state());
  q = tm_rewind(q, tm_new_state());

  int prev_pc = 0;
  for (Inst* inst = module->text; inst; inst = inst->next) {
    printf("// "); dump_inst_fp(inst, stdout);

    // If new pc, transition to state corresponding to new pc
    if (inst->pc != prev_pc && q != inst->pc)
      q = tm_noop(q, inst->pc);
    prev_pc = inst->pc;

    q = tm_rewind(q, tm_new_state());

    switch (inst->op) {

    case MOV:
      q = tm_copy_value(q, inst, TM_SKIP_BEFORE_DST, tm_new_state());
      break;

    case ADD:
    case SUB:
      q = tm_copy_value(q, inst, TM_SKIP_AFTER_DST, tm_new_state());
      // ok not to go to beginning of value
      if (inst->op == ADD)
	q = tm_addsub(q, +1, tm_new_state());
      else if (inst->op == SUB)
	q = tm_addsub(q, -1, tm_new_state());
      break;

    case LOAD:
      assert (inst->dst.type == REG);
      if (inst->src.type == REG) {
	q = tm_find_memory_indirect(q, inst->src.reg, tm_new_state());
      } else if (inst->src.type == IMM) {
	q = tm_find_memory(q, inst->src.imm, tm_new_state());
      } else
	error("invalid src type");
      q = tm_write(q, TM_SRC, -1, tm_new_state());
      q = tm_find_register(q, inst->dst.reg, tm_new_state());
      q = tm_write(q, TM_DST, +1, tm_new_state());
      q = tm_move(q, +1, tm_new_state());
      q = tm_find(q, +1, TM_SRC, tm_new_state(), tm_q_reject);
      q = tm_copy(q, -1, TM_SKIP_BEFORE_SRC|TM_SKIP_BEFORE_DST, tm_new_state());
      break;

    case STORE:
      assert (inst->dst.type == REG);
      if (inst->src.type == REG) {
	q = tm_find_memory_indirect(q, inst->src.reg, tm_new_state());
      } else if (inst->src.type == IMM) {
	q = tm_find_memory(q, inst->src.imm, tm_new_state());
      } else
	error("invalid dst type");
      q = tm_write(q, TM_DST, -1, tm_new_state());
      q = tm_move(q, +1, tm_new_state());
      q = tm_find_register(q, inst->dst.reg, tm_new_state());
      q = tm_copy(q, +1, TM_SKIP_BEFORE_SRC|TM_SKIP_BEFORE_DST, tm_new_state());
      break;

    case GETC:
      // Mark last 8 tm_bits of register as TM_DST
      assert (inst->dst.type == REG);
      q = tm_find_register(q, inst->dst.reg, tm_new_state());
      q = tm_write_tm_bits(q, 0, tm_word_size-8, TM_SKIP_BEFORE_DST, tm_new_state());
      q = tm_write(q, TM_DST, -1, tm_new_state());

      // Copy next 8 tm_bits of input, if any
      int q_eof = tm_new_state(), q_done = tm_new_state();
      q = tm_rewind(q, tm_new_state());
      q = tm_move(q, +1, tm_new_state()); // ^[_]...
      q = tm_move_if(q, TM_BLANK, +1, q, 0, tm_new_state()); // ...[0] or ...[r]
      q = tm_move_if(q, TM_REGISTER, 0, q_eof, +1, tm_new_state());
      for (int i=0; i<6; i++)
	q = tm_move(q, +1, tm_new_state());
      q = tm_insert(q, TM_BLANK, -1, tm_new_state());
      q = tm_move(q, +1, tm_new_state());
      q = tm_copy(q, +1, TM_SKIP_BEFORE_DST, q_done);

      // Go back to TM_DST and write zero byte indicating EOF
      q = q_eof;
      q = tm_find(q, +1, TM_DST, tm_new_state(), tm_q_reject);
      q = tm_write(q, TM_BLANK, 0, tm_new_state());
      q = tm_write_tm_bits(q, 0, 8, TM_SKIP_BEFORE_DST, q_done);

      q = q_done;
      
      break;

    case PUTC:
      q = tm_ffwd(q, tm_new_state());
      q = tm_write(q, TM_OUTPUT, +1, tm_new_state());
      if (inst->src.type == REG) {
	q = tm_write(q, TM_DST, -1, tm_new_state());
	q = tm_find_register(q, inst->src.reg, tm_new_state());
	for (int i=0; i<(tm_word_size-8)*2; i++)
	  q = tm_move(q, +1, tm_new_state());
	q = tm_copy(q, +1, TM_SKIP_BEFORE_SRC, tm_new_state());
      } else if (inst->src.type == IMM) {
	q = tm_write_tm_bits(q, inst->src.imm, 8, 0, tm_new_state());
      } else
	error("invalid src type");
      q = tm_write(q, TM_BLANK, +1, tm_new_state());
      q = tm_write(q, TM_END, 0, tm_new_state());
      break;

    case EXIT:
      // Consolidate output segments
      q = tm_write(q, TM_DST, +1, tm_new_state());
      int q_clear = tm_new_state();
      int q_findo = q;
      q = tm_find(q, +1, TM_OUTPUT, tm_new_state(), q_clear);
      q = tm_write(q, TM_BLANK, +1, tm_new_state());
      q = tm_copy(q, -1, 0, tm_new_state());
      tm_write(q, TM_DST, +1, q_findo);

      // Clear rest of tape
      q_clear = tm_ffwd(q_clear, tm_new_state());
      tm_write_if(q_clear, TM_DST, TM_BLANK, 0, -1, TM_BLANK, -1, q_clear);
      q = tm_new_state();
      break;

    case JEQ:
    case JNE:
    case JLT:
    case JGE:
    case JGT:
    case JLE:
      q = tm_copy_value(q, inst, TM_SKIP_AFTER_DST, tm_new_state());
      q = tm_find(q, -1, TM_VALUE, tm_new_state(), tm_q_reject);
      q = tm_move(q, +1, tm_new_state());
      if (inst->jmp.type == REG)
	error("jmp reg not implemented");
      else if (inst->jmp.type == IMM) {
	int q_false = tm_new_state();
	tm_compare(q, inst->op, inst->jmp.imm, q_false);
	q = q_false;
      } else
	error("invalid jmp type");
      break;

    case JMP:
      if (inst->jmp.type == REG)
	tm_find_register(q, inst->jmp.reg, q_jmpreg);
      else if (inst->jmp.type == IMM)
	tm_noop(q, inst->jmp.imm);
      else
	error("invalid jmp type");
      q = tm_new_state();
      break;

    case EQ:
    case NE:
    case LT:
    case GE:
    case GT:
    case LE:
      q = tm_copy_value(q, inst, TM_SKIP_AFTER_DST, tm_new_state());
      q = tm_find(q, -1, TM_VALUE, tm_new_state(), tm_q_reject);
      q = tm_move(q, +1, tm_new_state());
      int qt = tm_new_state(), qf = tm_new_state();
      q = tm_compare(q, inst->op, qt, qf);
      q = tm_new_state();
      qf = tm_find(qf, -1, TM_VALUE, tm_new_state(), tm_q_reject);
      qf = tm_move(qf, +1, tm_new_state());
      qf = tm_write_tm_bits(qf, 0, tm_word_size, TM_SKIP_BEFORE_DST, q);
      qt = tm_find(qt, -1, TM_VALUE, tm_new_state(), tm_q_reject);
      qt = tm_move(qt, +1, tm_new_state());
      qt = tm_write_tm_bits(qt, 1, tm_word_size, TM_SKIP_BEFORE_DST, q);
      break;

    case DUMP:
      break;

    default:
      error("invalid operation");
    }
  }
}
