#include <assert.h>
#include <ctype.h>
#include <ir/ir.h>
#include <target/util.h>

int next_state;
int new_state() {
  return next_state++;
}
int q_reject;

/* The tape is laid out like this:
     ^_00000...r_0_..._0_v_0_..._0_..._a_0_..._0_v_0_..._0_..._o000...0_...$
       input   register    value       address   value         output

   The blanks between the symbols are used as scratch space. */

typedef enum {BLANK, START, END, ZERO, ONE, REGISTER, ADDRESS, VALUE, OUTPUT, SRC, DST} symbol_t;
const char *symbol_names[] = {"_", "^", "$", "0", "1", "r", "a", "v", "o", "s", "d"};
const int num_symbols = 11;
const int bit[2] = {ZERO, ONE};

const int word_size = 10;

typedef enum {
  SKIP_BEFORE_SRC = 1,
  SKIP_AFTER_SRC = 2,
  SKIP_BEFORE_DST = 4,
  SKIP_AFTER_DST = 8
} mode_t;

int intcmp(int x, int y) {
  if (x > y) return +1;
  else if (x < y) return -1;
  else return 0;
}

void comment(const char* fmt, ...) {
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

int tm_transition(int q, symbol_t a, symbol_t b, int d, int r) {
  const char *dname;
  if (d == -1)      dname = "L";
  else if (d == 0)  dname = "N"; 
  else if (d == +1) dname = "R";
  else error("invalid direction %d", d);
  emit_line("%d %s %d %s %s", 
	    q, symbol_names[a], 
	    r, symbol_names[b], dname);
  return r;
}

/* Generate transitions to write symbol b and move in direction d,
   regardless of input symbol. */

int tm_write(int q, symbol_t b, int d, int r) {
  for (symbol_t a=0; a<num_symbols; a++)
    tm_transition(q, a, b, d, r);
  return r;
}

/* Generate write transitions that do one thing for symbol a, 
   and another thing for all other symbols.

   Returns the state for the latter case. */

int tm_write_if(int q, 
		symbol_t a, int ba, int da, int ra,
		symbol_t b, int d, int r) {
  for (symbol_t s=0; s<num_symbols; s++)
    if (s == a) tm_transition(q, s, ba, da, ra); 
    else        tm_transition(q, s, b, d, r);
  return r;
}

int tm_write_if2(int q, 
		 symbol_t a1, int b1, int d1, int r1,
		 symbol_t a2, int b2, int d2, int r2,
		 symbol_t b, int d, int r) {
  for (symbol_t s=0; s<num_symbols; s++)
    if (s == a1)      tm_transition(q, s, b1, d1, r1); 
    else if (s == a2) tm_transition(q, s, b2, d2, r2); 
    else              tm_transition(q, s, b, d, r);
  return r;
}

/* Generate transitions to move in direction d, regardless of input symbol. */

int tm_move(int q, int d, int r) {
  for (symbol_t s=0; s<num_symbols; s++)
    tm_transition(q, s, s, d, r);
  return r;
}

/* Generate move transitions that do one thing for symbol a, 
   and another thing for all other symbols.

   Returns the state for the latter case. */

int tm_move_if(int q, 
	       symbol_t a, int da, int ra, 
	       int d, int r) {
  for (symbol_t s=0; s<num_symbols; s++)
    if (s == a)      tm_transition(q, s, s, da, ra);
    else             tm_transition(q, s, s, d, r);
  return r;
}

/* Generate move transitions that do one thing for symbol a, 
   another thing for symbol b,
   and another thing for all other symbols.

   Returns the state for the last case. */

int tm_move_if2(int q, 
		symbol_t a, int da, int ra, 
		symbol_t b, int db, int rb, 
		int d, int r) {
  for (symbol_t s=0; s<num_symbols; s++)
    if (s == a)      tm_transition(q, s, s, da, ra);
    else if (s == b) tm_transition(q, s, s, db, rb);
    else             tm_transition(q, s, s, d, r);
  return r;
}

/* Generate transitions that just change state and do nothing else. */

int tm_noop(int q, int r) { 
  return tm_move(q, 0, r); 
}

/* Inserts a symbol and shifts existing bits to the left/right until a
   non-bit is reached. Afterwards, the head is on the first
   non-bit. */

int tm_insert(int q, symbol_t a, int d, int r) {
  int q0 = new_state(), q1 = new_state();
  tm_write_if2(q,  ZERO, a,    d, q0, ONE, a,    d, q1, a,    d, r);
  tm_write_if2(q0, ZERO, ZERO, d, q0, ONE, ZERO, d, q1, ZERO, d, r);
  tm_write_if2(q1, ZERO, ONE,  d, q0, ONE, ONE,  d, q1, ONE,  d, r);
  return r;
}

/* Generate transitions to write a binary number,
   leaving a scratch cell before each bit. */

int tm_write_bits(int q, unsigned int x, int n, int mode, int r) {
  for (int i=n-1; i>=0; i--) {
    if (mode & SKIP_BEFORE_DST)
      q = tm_move(q, +1, new_state());
    q = tm_write(q, (1<<i)&x?ONE:ZERO, +1, new_state());
    if (mode & SKIP_AFTER_DST)
      q = tm_move(q, +1, new_state());
  }
  return tm_noop(q, r);
}

int tm_erase_bits(int q, int r) {
  int erase = q, skip = new_state();
  tm_write_if2(erase, ZERO, BLANK, +1, skip, ONE, BLANK, +1, skip, BLANK, 0, r);
  tm_move(skip, +1, erase);
  return r;
}

int tm_write_word(int q, unsigned int x, int mode, int r) {
  return tm_write_bits(q, x, word_size, mode, r);
}

int tm_write_byte(int q, unsigned int x, int mode, int r) {
  return tm_write_bits(q, x, 8, mode, r);
}

/* Generate transitions to search in direction d for symbol a,
   or until the end of the used portion of the tape is reached. */

int tm_find(int q, int d, int a, int r_yes, int r_no) {
  symbol_t marker = d < 0 ? START : END;
  tm_move_if2(q, a, 0, r_yes, marker, 0, r_no, d, q);
  return r_yes;
}

/* Generate transitions to move to the left or right end of the tape. */

int tm_rewind(int q, int r) { 
  tm_move_if(q, START, 0, r, -1, q);
  return r;
}

int tm_ffwd(int q, int r) { 
  tm_move_if(q, END, 0, r, +1, q);
  return r;
}

/* Generate transitions to find register reg or address addr. The head
   ends on the scratch cell to the left of the corresponding value. */

int tm_find_location(int q, symbol_t type, int addr, int r_found, int r_notfound) {
  int qstart = q;
  q = tm_find(q, +1, type, new_state(), r_notfound);   // _[r]_0_1 ... _v_0_1
  q = tm_move(q, +1, new_state());                     // _r[_]0_1 ... _v_0_1
  for (int i=word_size-1; i>=0; i--) {
    q = tm_move(q, +1, new_state());                   // _r_[0]_1 ... _v_0_1
    symbol_t bit = (1<<i)&addr ? ONE : ZERO;
    int q_match = new_state();
    tm_move_if(q,
	       bit, +1, q_match,                       // _r_0[_]1 ... _v_0_1
	       +1, qstart);
    q = q_match;
  }
  q = tm_move(q, +1, new_state());                     // _r_0_1 ... _[v]_0_1
  tm_move_if(q,
	     VALUE, +1, r_found,                       // _r_0_1 ... _v[_]0_1
	     0, q_reject);
  return r_found;
}

int tm_new_location(int q, symbol_t type, int addr, int r) {
  q = tm_ffwd(q, new_state());
  q = tm_write(q, type, +1, new_state());
  q = tm_move(q, +1, new_state());
  q = tm_write_word(q, addr, SKIP_AFTER_DST, new_state());
  q = tm_write(q, VALUE, +1, new_state());
  q = tm_move(q, +1, new_state());
  q = tm_write_word(q, 0, SKIP_AFTER_DST, new_state());
  return tm_write(q, END, -1, r);
}

int tm_find_register(int q, Reg reg, int r) {
  q = tm_rewind(q, new_state()); // to do: can be slow if input is long
  return tm_find_location(q, REGISTER, reg, r, q_reject);
}

int tm_find_memory(int q, int addr, int r) {
  int q_append = new_state();
  tm_find_location(q, ADDRESS, addr, r, q_append);
  q = tm_new_location(q_append, ADDRESS, addr, new_state());
  q = tm_find(q, -1, VALUE, new_state(), q_reject);
  tm_move(q, +1, r);
  return r;
}

/* Copy bits from current position to the position marked by DST.

   The head starts on the scratch cell to the left of the source word,
   and ends on the cell to the right of the destination word (which is
   erased). */

int tm_copy(int q, int d, unsigned int mode, int r) {
  // The examples show mode == SKIP_BEFORE_SRC|SKIP_AFTER_DST.
  int q_store[2] = {new_state(), new_state()};
  int q_cleansrc = new_state(), q_cleandst = new_state();
  int q_nextbit = q;                                  

  if (mode & SKIP_BEFORE_SRC) {
    // Write SRC, then store bit
                                                        // [_]0_1 ... d_x
    q = tm_write(q, SRC, 0, new_state());               // [s]0_1 ... d_x
    q = tm_move(q, +1, new_state());                    // s[0]_1 ... d_x
    tm_move_if2(q,
		ZERO, 0, q_store[0],                    // s[0]_1 ... d_x
		ONE,  0, q_store[1],
		0, q_cleansrc);
  } else {
    // Store bit, then write SRC
    tm_write_if2(q, 
		 ZERO, SRC, 0, q_store[0], 
		 ONE, SRC, 0, q_store[1], 
		 BLANK, 0, q_cleandst);
  }
  int q_write = new_state();
  for (int b=0; b<2; b++) {
    q = q_store[b];
    q = tm_find(q, d, DST, new_state(), q_reject);    // s0_1 ... [d]_x
    if (mode & SKIP_BEFORE_DST)
      q = tm_write(q, BLANK, +1, new_state());
    tm_write(q, bit[b], +1, q_write);                 // s0_1 ... 0[_]x
  }
  q = q_write;
  if (mode & SKIP_AFTER_DST)
    q = tm_move(q, +1, new_state());                  // s0_1 ... 0[_]x
  q = tm_write(q, DST, 0, new_state());               // s0_1 ... 0[d]x
  q = tm_find(q, -d, SRC, new_state(), q_reject);     // [s]0_1 ... 0dx
  q = tm_write(q, BLANK, +1, new_state());            // _[0]_1 ... 0dx
  if (mode & (SKIP_BEFORE_SRC|SKIP_AFTER_SRC))
    tm_move(q, +1, q_nextbit);                        // _0[_]1 ... 0dx
  else
    tm_noop(q, q_nextbit);

  q = q_cleansrc;
  q = tm_move(q, -1, new_state());
  q = tm_write(q, BLANK, 0, q_cleandst);
  q = tm_find(q, d, DST, new_state(), q_reject);
  return tm_write(q, BLANK, 0, r);
}

/* Gets value from inst->src and copies it to inst->dst with mode
   indicated by mode. */

int tm_copy_value(int q, Inst *inst, int mode, int r) {
  assert(inst->dst.type == REG);
  q = tm_find_register(q, inst->dst.reg, new_state()); // v[_]x_x...
  if (inst->src.type == REG) {
    if (inst->dst.reg == inst->src.reg) {
      if (mode == SKIP_AFTER_DST) {
	// Copy each bit to the cell to its left
	int q_end = new_state(), q_next = q;
	q = tm_move(q, +1, new_state());
	tm_move_if2(q, ZERO, +1, q_next, ONE, +1, q_next, -1, q_end);
	q = q_end;
	q = tm_move(q, -1, new_state());
	q_next = q;
	int q0 = new_state(), q1 = new_state();
	q_end = new_state();
	tm_move_if2(q, ZERO, -1, q0, ONE, -1, q1, +1, q_end);
	tm_write(q0, ZERO, -1, q_next);
	tm_write(q1, ONE,  -1, q_next);
	q = q_end;
	tm_move_if2(q, ZERO, +1, q, ONE, +1, q, 0, r);
      } else {
	// Do nothing
	tm_noop(q, r);
      }
    } else {
      q = tm_write(q, DST, -1, new_state());
      q = tm_find_register(q, inst->src.reg, new_state());
      q = tm_copy(q, intcmp(inst->dst.reg, inst->src.reg), SKIP_BEFORE_SRC|mode, r);
    }
  } else if (inst->src.type == IMM) {
    q = tm_write_word(q, inst->src.imm, mode, r);
  } else
    error("invalid src type");
  return r;
}

/* Add/subtract binary number in scratch cells to/from binary number
   in main cells, while erasing the scratch cells. */

int tm_addsub(int q, int c, int r) {
  assert (c == -1 || c == +1);

  // Go to end of numbers
  int q_end = new_state();
  q = tm_move_if2(q, ZERO, +1, q, ONE, +1, q, -1, q_end);

  // Perform the operation, putting result in scratch cells

  // Carry bit
  int carry[2] = {q, new_state()};
  if (c == -1) { int tmp = carry[0]; carry[0] = carry[1]; carry[1] = tmp; }

  // Carry bit plus bit from first number
  int inter[3] = {new_state(), new_state(), new_state()};

  int q_shift = new_state();
  for (int a=0; a<2; a++) {
    tm_transition(carry[a], ZERO,  BLANK, -1, inter[a]);
    tm_transition(carry[a], ONE,   BLANK, -1, inter[a+1]);
    tm_transition(carry[a], VALUE, VALUE, +1, q_shift);
  }
  for (int a=0; a<3; a++)
    for (int b=0; b<2; b++) {
      tm_transition(inter[a], bit[c == +1 ? b : !b], bit[(a+b)&1], -1, carry[(a+b)>>1]);
    }
  c = 0;

  // Shift scratch cells to main cells
  int q0 = new_state(), q1 = new_state();
  q = q_shift;
  tm_write_if2(q, ZERO, BLANK, +1, q0, ONE, BLANK, +1, q1, BLANK, 0, r);
  q0 = tm_write(q0, ZERO, +1, q);
  q1 = tm_write(q1, ONE,  +1, q);
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

  int q_nextbit = q;
  int q0 = new_state(), q1 = new_state();
  int erase_gt = new_state(), erase_lt = new_state();
  tm_write_if2(q, ZERO, BLANK, +1, q0, ONE, BLANK, +1, q1, BLANK, 0, q_eq);
  tm_move_if2(q0, ZERO, +1, q_nextbit, ONE, +1, erase_gt, 0, q_reject);
  tm_move_if2(q1, ZERO, +1, erase_lt, ONE, +1, q_nextbit, 0, q_reject);
  tm_erase_bits(erase_lt, q_lt);
  tm_erase_bits(erase_gt, q_gt);
  return r_true;
}

/* Similar, but doesn't erase the scratch cells. */

int tm_equal(int q, int r_eq, int r_ne) {
  int q_nextbit = q;
  int q0 = new_state(), q1 = new_state();
  tm_move_if2(q, ZERO, +1, q0, ONE, +1, q1, 0, r_eq);
  tm_move_if2(q0, ZERO, +1, q_nextbit, ONE, +1, r_ne, 0, q_reject);
  tm_move_if2(q1, ZERO, +1, r_ne, ONE, +1, q_nextbit, 0, q_reject);
  return r_eq;
}

/* Reads address from current location on tape, and finds the memory
   location with that address, creating a new memory location if
   necessary. */

int tm_find_memory_indirect(int q, int reg, int r) {
  // mark first address as DST
  int q_append = new_state();
  q = tm_find(q, +1, ADDRESS, new_state(), q_append);
  q = tm_move(q, +1, new_state());
  q = tm_write(q, DST, -1, new_state());
  q = tm_find_register(q, reg, new_state());

  q = tm_copy(q, +1, SKIP_BEFORE_SRC|SKIP_AFTER_DST, new_state()); // _axyxyxy...[_]v_z_z_z...

  // compare addresses
  int q_nextaddr = new_state();
  int q_compare = q;
  q = tm_find(q, -1, ADDRESS, new_state(), q_reject);
  q = tm_move(q, +1, new_state());
  q = tm_equal(q, new_state(), q_nextaddr);

  // if equal: move right to value
  q = tm_move(q, +1, new_state());
  tm_move(q, +1, r);

  // if not equal: 
  // mark next address as DST
  q = q_nextaddr;
  q = tm_find(q, +1, ADDRESS, new_state(), q_append);
  q = tm_move(q, +1, new_state());
  q = tm_write(q, DST, -1, new_state());

  // return to previous address
  q = tm_move(q, -1, new_state());
  q = tm_move(q, -1, new_state());
  q = tm_find(q, -1, ADDRESS, new_state(), q_reject);
  q = tm_move(q, +1, new_state());
  
  // move address
  tm_copy(q, +1, SKIP_AFTER_SRC|SKIP_AFTER_DST, q_compare);

  // if no more addresses: create new memory location
  q = q_append;
  q = tm_write(q, ADDRESS, +1, new_state());
  q = tm_write(q, BLANK, +1, new_state());
  q = tm_write(q, DST, -1, new_state());
  q = tm_move(q, -1, new_state());
  q = tm_move(q, -1, new_state());
  q = tm_find(q, -1, ADDRESS, new_state(), q_reject);
  q = tm_move(q, +1, new_state());
  q = tm_copy(q, +1, SKIP_AFTER_SRC|SKIP_AFTER_DST, new_state());
  q = tm_move(q, +1, new_state());
  q = tm_write(q, VALUE, +1, new_state());
  q = tm_move(q, +1, new_state());
  q = tm_write_word(q, 0, SKIP_AFTER_DST, new_state());
  q = tm_write(q, END, -1, new_state());
  q = tm_find(q, -1, VALUE, new_state(), q_reject);
  tm_move(q, +1, r);
  return r;
}

void target_tm(Module* module) {
  /* Every basic block's entry point is the state with the same number
     as its pc. Additional states are numbered starting after the
     highest pc. */
  next_state = 0;
  for (Inst* inst = module->text; inst; inst = inst->next)
    if (inst->pc >= next_state)
      next_state = inst->pc+1;

  q_reject = new_state();
  int q = 0; // start state (pc 0)

  comment("beginning-of-tape and input string");

  // Insert START and a single scratch cell before input string
  q = tm_insert(q, START, +1, new_state());
  q = tm_rewind(q, new_state());
  q = tm_move(q, +1, new_state());
  q = tm_insert(q, BLANK, +1, new_state());
  q = tm_write(q, END, 0, new_state());

  // Initialize registers
  for (Reg reg=0; reg<6; reg++) {
    comment("register %s value 0", reg_names[reg]);
    q = tm_new_location(q, REGISTER, reg, new_state());
  }

  // Initialize memory
  Data *data = module->data;
  int mp = 0;
  while (data) {
    if (isprint(data->v))
      comment("address %d value %d '%c'", mp, data->v, data->v);
    else
      comment("address %d value %d", mp, data->v);
    q = tm_new_location(q, ADDRESS, mp, new_state());
    data = data->next;
    mp++;
  }
  q = tm_write(q, BLANK, +1, new_state());
  q = tm_write(q, END, -1, new_state());
  q = tm_rewind(q, new_state());

  int prev_pc = 0;
  for (Inst* inst = module->text; inst; inst = inst->next) {
    printf("// "); dump_inst_fp(inst, stdout);

    // If new pc, transition to state corresponding to new pc
    if (inst->pc != prev_pc && q != inst->pc)
      q = tm_noop(q, inst->pc);
    prev_pc = inst->pc;

    q = tm_rewind(q, new_state());

    switch (inst->op) {

    case MOV:
      q = tm_copy_value(q, inst, SKIP_BEFORE_DST, new_state());
      break;

    case ADD:
    case SUB:
      q = tm_copy_value(q, inst, SKIP_AFTER_DST, new_state());
      // ok not to go to beginning of value
      if (inst->op == ADD)
	q = tm_addsub(q, +1, new_state());
      else if (inst->op == SUB)
	q = tm_addsub(q, -1, new_state());
      break;

    case LOAD:
      assert (inst->dst.type == REG);
      if (inst->src.type == REG) {
	q = tm_find_memory_indirect(q, inst->src.reg, new_state());
      } else if (inst->src.type == IMM) {
	q = tm_find_memory(q, inst->src.imm, new_state());
      } else
	error("invalid src type");
      q = tm_write(q, SRC, -1, new_state());
      q = tm_find_register(q, inst->dst.reg, new_state());
      q = tm_move(q, +1, new_state());
      q = tm_write(q, DST, +1, new_state());
      q = tm_find(q, +1, SRC, new_state(), q_reject);
      q = tm_copy(q, -1, SKIP_BEFORE_SRC|SKIP_AFTER_DST, new_state());
      break;

    case STORE:
      assert (inst->dst.type == REG);
      if (inst->src.type == REG) {
	q = tm_find_memory_indirect(q, inst->src.reg, new_state());
      } else if (inst->src.type == IMM) {
	q = tm_find_memory(q, inst->src.imm, new_state());
      } else
	error("invalid dst type");
      q = tm_write(q, DST, -1, new_state());
      q = tm_find_register(q, inst->dst.reg, new_state());
      q = tm_move(q, +1, new_state());
      q = tm_copy(q, +1, SKIP_BEFORE_SRC|SKIP_AFTER_DST, new_state());
      break;

    case GETC:
      assert (inst->dst.type == REG);
      for (int i=0; i<9; i++)
	q = tm_move(q, +1, new_state());
      q = tm_insert(q, BLANK, -1, new_state());
      q = tm_find_register(q, inst->dst.reg, new_state());
      for (int i=0; i<(word_size-8); i++) {
	q = tm_move(q, +1, new_state());
	q = tm_write(q, ZERO, +1, new_state());
      }
      q = tm_write(q, DST, -1, new_state());
      q = tm_rewind(q, new_state());
      q = tm_move(q, +1, new_state());
      q = tm_copy(q, +1, SKIP_BEFORE_DST, new_state());
      q = tm_rewind(q, new_state());
      q = tm_write(q, BLANK, +1, new_state());
      int q_writestart = new_state();
      q = tm_move_if(q, BLANK, +1, q, 0, q_writestart);
      q = tm_move(q, -1, new_state());
      q = tm_move(q, -1, new_state());
      q = tm_write(q, START, 0, new_state());
      break;

    case PUTC:
      q = tm_ffwd(q, new_state());
      q = tm_write(q, OUTPUT, +1, new_state());
      if (inst->src.type == REG) {
	q = tm_write(q, DST, -1, new_state());
	q = tm_find_register(q, inst->src.reg, new_state());
	for (int i=0; i<(word_size-8)*2; i++)
	  q = tm_move(q, +1, new_state());
	q = tm_copy(q, +1, SKIP_BEFORE_SRC, new_state());
      } else if (inst->src.type == IMM) {
	q = tm_write_byte(q, inst->src.imm, 0, new_state());
      } else
	error("invalid src type");
      q = tm_write(q, BLANK, +1, new_state());
      q = tm_write(q, END, 0, new_state());
      break;

    case EXIT:
      // Consolidate output segments
      q = tm_write(q, DST, +1, new_state());
      int q_clear = new_state();
      int q_findo = q;
      q = tm_find(q, +1, OUTPUT, new_state(), q_clear);
      q = tm_write(q, BLANK, +1, new_state());
      q = tm_copy(q, -1, 0, new_state());
      tm_write(q, DST, +1, q_findo);

      // Clear rest of tape
      q_clear = tm_ffwd(q_clear, new_state());
      tm_write_if(q_clear, DST, BLANK, 0, -1, BLANK, -1, q_clear);
      q = new_state();
      break;

    case JEQ:
    case JNE:
    case JLT:
    case JGE:
    case JGT:
    case JLE:
      q = tm_copy_value(q, inst, SKIP_AFTER_DST, new_state());
      q = tm_find(q, -1, VALUE, new_state(), q_reject);
      q = tm_move(q, +1, new_state());
      if (inst->jmp.type == REG)
	error("jmp reg not implemented");
      else if (inst->jmp.type == IMM)
	q = tm_compare(q, inst->op, inst->jmp.imm, new_state());
      else
	error("invalid jmp type");
      break;

    case JMP:
      if (inst->jmp.type == REG)
	error("jmp reg not implemented");
      else if (inst->jmp.type == IMM)
	q = tm_noop(q, new_state());
      else
	error("invalid jmp type");
      break;

    case EQ:
    case NE:
    case LT:
    case GE:
    case GT:
    case LE:
      q = tm_copy_value(q, inst, SKIP_AFTER_DST, new_state());
      q = tm_find(q, -1, VALUE, new_state(), q_reject);
      q = tm_move(q, +1, new_state());
      int qt = new_state(), qf = new_state();
      q = tm_compare(q, inst->op, qt, qf);
      q = new_state();
      qf = tm_find(qf, -1, VALUE, new_state(), q_reject);
      qf = tm_write_word(qf, 0, SKIP_AFTER_DST, q);
      qt = tm_find(qt, -1, VALUE, new_state(), q_reject);
      qt = tm_write_word(qt, 1, SKIP_AFTER_DST, q);
      break;

    case DUMP:
      break;

    default:
      error("invalid operation");
    }
  }
}
