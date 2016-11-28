#include <assert.h>
#include <ctype.h>
#include <ir/ir.h>
#include <target/util.h>

typedef enum {BLANK, START, ZERO, ONE, REGISTER, ADDRESS, VALUE, OUTPUT, SRC, DST, SCRATCH} symbol_t;
const char *symbol_names[] = {"_", "^", "0", "1", "r", "a", "v", "o", "s", "d", "."};
const int num_symbols = 11;

const char *direction_names[] = {"L", "N", "R"};

const int word_size = 8;

int next_state;

int new_state() {
  return next_state++;
}

/* These functions take a start state and an accept state(s) as
   arguments and, as a convenience, return the accept state. */

int tm_transition(int q, symbol_t a, symbol_t b, int d, int r) {
  emit_line("%d %s %d %s %s", 
	    q, symbol_names[a], 
	    r, symbol_names[b], direction_names[d+1]);
  return r;
}

int tm_write(int q, symbol_t b, int d, int r) {
  for (symbol_t a=0; a<num_symbols; a++)
    tm_transition(q, a, b, d, r);
  return r;
}

int tm_write_if(int q, 
		symbol_t a, int ba, int da, int ra,
		symbol_t b, int d, int r) {
  for (symbol_t s=0; s<num_symbols; s++)
    if (s == a) tm_transition(q, s, ba, da, ra); 
    else        tm_transition(q, s, b, d, r);
  return r;
}

int tm_move(int q, int d, int r) {
  for (symbol_t s=0; s<num_symbols; s++)
    tm_transition(q, s, s, d, r);
  return r;
}

int tm_move_if(int q, 
	       symbol_t a, int da, int ra, 
	       int d, int r) {
  for (symbol_t s=0; s<num_symbols; s++)
    if (s == a)      tm_transition(q, s, s, da, ra);
    else             tm_transition(q, s, s, d, r);
  return r;
}

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

int tm_noop(int q, int r) { 
  return tm_move(q, 0, r); 
}

int tm_write_word(int q, unsigned int x, int r) {
  for (int i=word_size-1; i>0; i--) {
    q = tm_write(q, SCRATCH, +1, new_state());
    q = tm_write(q, (1<<i)&x?ONE:ZERO, +1, new_state());
  }
  q = tm_write(q, SCRATCH, +1, new_state());
  return tm_write(q, 1&x?ONE:ZERO, +1, r);
}

int tm_write_byte(int q, unsigned int x, int r) {
  for (int i=7; i>0; i--) {
    q = tm_write(q, SCRATCH, +1, new_state());
    q = tm_write(q, (1<<i)&x?ONE:ZERO, +1, new_state());
  }
  q = tm_write(q, SCRATCH, +1, new_state());
  return tm_write(q, 1&x?ONE:ZERO, +1, r);
}

int tm_find_left(int q, int a, int r_yes, int r_no) {
  tm_move_if2(q, a, 0, r_yes, START, 0, r_no, -1, q);
  return r_yes;
}

int tm_find_right(int q, int a, int r_yes, int r_no) {
  tm_move_if2(q, a, 0, r_yes, BLANK, 0, r_no, +1, q);
  return r_yes;
}

int tm_rewind(int q, int r) { 
  tm_move_if(q, START, 0, r, -1, q);
  return r;
}

int tm_ffwd(int q, int r) { 
  tm_move_if(q, BLANK, 0, r, +1, q);
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

  printf("// beginning-of-tape marker\n");
  int q_reject = new_state();
  int q = 0; // current state
  q = tm_write(q, START, +1, new_state());

  // Initialize registers
  for (Reg reg=0; reg<6; reg++) {
    printf("\n// register %s value 0\n", reg_names[reg]);
    q = tm_write(q, SCRATCH, +1, new_state());
    q = tm_write(q, REGISTER, +1, new_state());
    q = tm_write_word(q, reg, new_state());
    q = tm_write(q, SCRATCH, +1, new_state());
    q = tm_write(q, VALUE, +1, new_state());
    q = tm_write_word(q, 0, new_state());
  }

  // Initialize memory
  Data *data = module->data;
  int mp = 0;
  while (data) {
    if (isprint(data->v))
      printf("\n// address %d value %d '%c'\n", mp, data->v, data->v);
    else
      printf("\n// address %d value %d\n", mp, data->v);
    q = tm_write(q, SCRATCH, +1, new_state());
    q = tm_write(q, ADDRESS, +1, new_state());
    q = tm_write_word(q, mp, new_state());
    q = tm_write(q, SCRATCH, +1, new_state());
    q = tm_write(q, VALUE, +1, new_state());
    q = tm_write_word(q, data->v, new_state());
    data = data->next;
    mp++;
  }
  q = tm_rewind(q, new_state());

  int prev_pc = 0;
  for (Inst* inst = module->text; inst; inst = inst->next) {
    printf("\n// "); dump_inst_fp(inst, stdout);

    // If new pc, transition to state corresponding to new pc
    if (inst->pc != prev_pc && q != inst->pc)
      q = tm_noop(q, inst->pc);
    prev_pc = inst->pc;

    switch (inst->op) {

    case JMP:
      if (inst->jmp.type == REG)
	error("not implemented");
      else if (inst->jmp.type == IMM)
	q = tm_noop(q, inst->jmp.imm);
      else
	error("invalid jmp type");
      break;

    case PUTC:
      if (inst->src.type == REG)
	error("not implemented");
      else if (inst->src.type == IMM) {
	q = tm_ffwd(q, new_state());
	q = tm_write(q, SCRATCH, +1, new_state());
	q = tm_write(q, OUTPUT, +1, new_state());
	q = tm_write_byte(q, inst->src.imm, new_state());
	q = tm_rewind(q, new_state());
      } else
	error("invalid src type");
      break;

    case EXIT:
      // Consolidate output segments
      // Here (and only here), the destination has no scratch space
      // so a DST symbol marks the first unwritten cell
      q = tm_write(q, DST, +1, new_state());              // d[.]...
      int q_clear = new_state();
      int q_findoutput = q;
      q = tm_find_right(q, OUTPUT, new_state(), q_clear); // ...[o].0.1...
      q = tm_move(q, +1, new_state());                    // ...o[.]0.1...
      q = tm_write(q, SRC, 0, new_state());               // ...o[s]0.1...

      int q_nextbit = q;
      q = tm_write(q, SCRATCH, +1, new_state());          // ...o.[0].1...
      int q0 = new_state(), q1 = new_state();
      tm_move_if2(q, 
		  ZERO, +1, q0,                           // ...o.0[.]1... 
		  ONE, +1, q1,
		  0, q_findoutput);

      q = new_state();
      q0 = tm_write(q0, SRC, -1, new_state());            // ...o.[0]s1... 
      q0 = tm_find_left(q0, DST, new_state(), q_reject);  // [d]...
      q0 = tm_write(q0, ZERO, +1, q);                     // 0[?]..
      q1 = tm_write(q1, SRC, -1, new_state());
      q1 = tm_find_left(q1, DST, new_state(), q_reject);
      q1 = tm_write(q1, ONE, +1, q);
      q = tm_write(q, DST, +1, new_state());              // 0d[?]...
      q = tm_find_right(q, SRC, q_nextbit, q_reject);     // ...o.0[s]1... 

      // Clear rest of tape
      q_clear = tm_find_left(q_clear, DST, new_state(), q_reject);
      q_clear = tm_write_if(q_clear, 
			    BLANK, BLANK, 0, -1,
			    BLANK, +1, q_clear);
      break;

    case DUMP:
      break;

    default:
      error("not implemented");
    }
  }
}
