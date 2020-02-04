#include <ir/ir.h>
#include <target/util.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

// The commands on the operations ring
typedef enum {
    OP_NOOP,
    OP_EXIT,
    OP_ONE,
    OP_ZERO,
    OP_LOAD,
    OP_STORE,
    OP_PADD,
    OP_DADD,
    OP_LOGIC,
    OP_IF,
    OP_INT_IO,
    OP_ASC_IO,
} OpCmd;

// The commands on the math ring
typedef enum {
    MATH_NOOP,
    MATH_LOAD,
    MATH_STORE,
    MATH_ADD,
    MATH_MULT,
    MATH_DIV,
    MATH_ZERO,
    MATH_LESS,
    MATH_GREATER,
    MATH_EQUAL,
    MATH_NOT,
    MATH_NEG,
} MathCmd;

#define NUM_CMDS_PER_RING 12

typedef enum {
    MATH_RING,
    OPERATION_RING,
} WhirlRing;

typedef enum {
    CLOCKWISE,
    COUNTERCLOCKWISE,
} RingDirection;

// Struct to keep track of the current state of the command rings
typedef struct {
    // Which ring is "active"
    WhirlRing active_ring;

    // Which command in the operations ring is currently selected
    OpCmd cur_op_pos;

    // Which command in the math ring is currently selected
    MathCmd cur_math_pos;

    // The direction of the operations ring
    RingDirection op_dir;

    // The direction of the math ring
    RingDirection math_dir;

    // Whether the last instruction was a zero AND that zero did not trigger
    // an execution
    bool last_is_zero;
} RingState;

typedef struct CodeSegment_ {
    // The ELVM instruction that this code segment implements
    Inst *inst;

    // The generated whirl code for this segment
    char *code;

    // The length of the code string, and how much space has been allocated
    // for it
    size_t alloc, len;

    // The length of this code segment, and all of the segments that follow
    // this segment. We need to know this so that jump instructions know how
    // far to jump forward in the code to reach the jump table.
    size_t total_code_len;

    struct CodeSegment_ *next, *prev;
} WhirlCodeSegment;

#define INIT_CODE_ALLOC 32

static const int UINT24_LIMIT = 1 << 24;

// Memory layout:
// TMP A B C D BP SP PC TMP TMP MEM0 TMP MEM1 TMP MEM2 ...
// Note that after an ELVM instruction has been executed, the memory
// position is the first memory cell

// The position in memory where the registers start
static const int REG_START_POS  = 1;

// The number for the PC register
static const int PC_REG_NUM = 6;

// The position of the extra temp position, after the registers in memory
static const int EXTRA_TMP1 = 8;

// The position in memory where the program's memory starts
static const int DATA_START = 10;

static WhirlCodeSegment *new_segment(Inst *inst, WhirlCodeSegment *prev) {
    WhirlCodeSegment *segment = malloc(sizeof(WhirlCodeSegment));
    segment->inst = inst;
    segment->code = malloc(sizeof(char) * INIT_CODE_ALLOC);
    segment->alloc = INIT_CODE_ALLOC;
    segment->len = 0;
    segment->total_code_len = 0;
    segment->next = NULL;
    segment->prev = prev;
    if (prev != NULL) {
        prev->next = segment;
    }
    return segment;
}

static void output_segments(const WhirlCodeSegment *segment) {
    int cur_col = 0;

    while (segment != NULL) {
        for (size_t i = 0; i < segment->len; i++) {
            putchar(segment->code[i]);    

            cur_col++;
            if (cur_col == 50) {
                putchar('\n');
                cur_col = 0;
            }
        }

        segment = segment->next;
    }

    putchar('\n');
}

static void free_segments(WhirlCodeSegment *head) {
    while (head != NULL) {
        WhirlCodeSegment *tmp = head;
        head = head->next;
        free(tmp->code);
        free(tmp);
    }
}

static void emit_instruction(WhirlCodeSegment *segment, char c) {
    if (segment->len == segment->alloc) {
        char *old_str = segment->code;
        segment->alloc *= 2;
        segment->code = malloc(sizeof(char) * segment->alloc);
        memcpy(segment->code, old_str, segment->len * sizeof(char));
        free(old_str);
    }
    segment->code[segment->len] = c;
    segment->len++;
}

static RingDirection reverse_direction(RingDirection dir) {
    return dir == CLOCKWISE ? COUNTERCLOCKWISE : CLOCKWISE;
}

// Adds a 1 instruction to the current code segment, and update the current
// ring state to account for it
static void emit_one(WhirlCodeSegment *segment, RingState *state) {
    if (state->active_ring == MATH_RING) {
        if (state->math_dir == CLOCKWISE) {
            state->cur_math_pos++;
            if (state->cur_math_pos == NUM_CMDS_PER_RING) {
                state->cur_math_pos = 0;
            }
        }
        else {
            if (state->cur_math_pos == 0) {
                state->cur_math_pos = NUM_CMDS_PER_RING;
            }
            state->cur_math_pos--;
        }
    }
    else {
        if (state->op_dir == CLOCKWISE) {
            state->cur_op_pos++;
            if (state->cur_op_pos == NUM_CMDS_PER_RING) {
                state->cur_op_pos = 0;
            }
        }
        else {
            if (state->cur_op_pos == 0) {
                state->cur_op_pos = NUM_CMDS_PER_RING;
            }
            state->cur_op_pos--;
        }
    }

    state->last_is_zero = false;
    emit_instruction(segment, '1');    
}

// Adds a 0 instruction to the current code segment, and update the current
// ring state to account for it
static void emit_zero(WhirlCodeSegment *segment, RingState *state) {
    if (state->active_ring == MATH_RING) {
        state->math_dir = reverse_direction(state->math_dir);
    }
    else {
        state->op_dir = reverse_direction(state->op_dir);
    }

    if (state->last_is_zero) {
        state->active_ring = (state->active_ring == MATH_RING ? OPERATION_RING :
                                                                MATH_RING);
    }

    state->last_is_zero = !state->last_is_zero;
    emit_instruction(segment, '0');
}

// Returns whether we should switch the direction of the current ring, based on
// which direction would generate shorter code for the target command we want
static bool should_switch(int cur_pos, int target_pos, RingDirection dir) {
    int ones_required;

    if (dir == CLOCKWISE) {
        ones_required = target_pos - cur_pos;
    }
    else {
        ones_required = cur_pos - target_pos;
    }

    if (ones_required < 0) {
        ones_required += NUM_CMDS_PER_RING;
    }

    return ones_required > (NUM_CMDS_PER_RING / 2);
}

static void generate_math_command(WhirlCodeSegment *segment, RingState *state, MathCmd cmd);

// Generates the Whirl code to execute a command on the operations ring, taking
// into account the current state of the command rings
static void generate_op_command(WhirlCodeSegment *segment, RingState *state, OpCmd cmd) {
    if (state->active_ring == MATH_RING) {
        // If we're on the wrong ring, we need to execute a nop command to switch to
        // the operations ring
        generate_math_command(segment, state, MATH_NOOP);
    }

    if (should_switch(state->cur_op_pos, cmd, state->op_dir)) {
        emit_zero(segment, state);
    }

    while (state->cur_op_pos != cmd) {
        emit_one(segment, state);
    }

    emit_zero(segment, state);
    emit_zero(segment, state);
}

// Generates the Whirl code to execute a command on the math ring, taking into
// account the current state of the command rings
static void generate_math_command(WhirlCodeSegment *segment, RingState *state, MathCmd cmd) {
    if (state->active_ring == OPERATION_RING) {
        // If we're on the wrong ring, we need to execute a nop command to switch to
        // the math ring
        generate_op_command(segment, state, OP_NOOP);
    }

    if (should_switch(state->cur_math_pos, cmd, state->math_dir)) {
        emit_zero(segment, state);
    }

    while (state->cur_math_pos != cmd) {
        emit_one(segment, state);
    }

    emit_zero(segment, state);
    emit_zero(segment, state);
}

// Resets the rings so that they're in the state that we're expecting after
// executing a jump instruction. The state we expect is: math ring is selected,
// both rings are going clockwise, and the selected commands are MATH_NOP and OP_IF
static void reset_to_after_jump(WhirlCodeSegment *segment, RingState *state) {
    generate_op_command(segment, state, OP_ZERO);
    generate_op_command(segment, state, OP_STORE);

    if (state->cur_math_pos != MATH_NOOP || state->math_dir != CLOCKWISE) {
        if (state->active_ring == OPERATION_RING) {
            emit_zero(segment, state);
            emit_zero(segment, state);
        }
        if (state->cur_math_pos == MATH_NOOP) {
            emit_one(segment, state);
            emit_zero(segment, state);
            emit_one(segment, state);
        }
        else {
            if (state->math_dir == COUNTERCLOCKWISE) {
                emit_zero(segment, state);
            }
            while (state->cur_math_pos != MATH_NOOP) {
                emit_one(segment, state);
            }
        }
    }

    if (state->active_ring == MATH_RING) {
        emit_zero(segment, state);
        emit_zero(segment, state);
    }

    if (state->op_dir != CLOCKWISE) {
        if (state->cur_op_pos == OP_IF) {
            emit_one(segment, state);
            emit_zero(segment, state);
            emit_one(segment, state);
        }
        else {
            emit_zero(segment, state);
        }
    }

    while (state->cur_op_pos != OP_IF) {
        emit_one(segment, state);
    }

    emit_zero(segment, state);
    emit_zero(segment, state);
}

// Sets the current memory position to a specific value. Preserves the value in
// the operations ring
static void set_mem(WhirlCodeSegment *segment, RingState *state, int val) {
    generate_math_command(segment, state, MATH_ZERO);
    generate_math_command(segment, state, MATH_STORE);

    if (val == 0) {
        return;
    }

    bool is_negative = val < 0;
    if (is_negative) {
        val = -val;
    }

    generate_math_command(segment, state, MATH_EQUAL);
    generate_math_command(segment, state, MATH_STORE);

    if (val == 1) {
        if (is_negative) {
            generate_math_command(segment, state, MATH_NEG);
            generate_math_command(segment, state, MATH_STORE);
        }
        return;
    }

    int highest_set_bit = 0;
    for (int i = val; i != 0; i >>= 1) {
        highest_set_bit++;
    }

    // We subtract two from the highest bit because we shift one less than
    // the highest bit to get to the most significant bit, and we have
    // already handled the first bit, so we subtract one again
    highest_set_bit -= 2;

    for (int mask = 1 << highest_set_bit; mask != 0; mask >>= 1) {
        generate_math_command(segment, state, MATH_ZERO);
        if ((val & mask) != 0) {
            generate_math_command(segment, state, MATH_LESS);
        }
        generate_math_command(segment, state, MATH_ADD);
        generate_math_command(segment, state, MATH_ADD);
        generate_math_command(segment, state, MATH_STORE);
    }

    if (is_negative) {
        generate_math_command(segment, state, MATH_NEG);
        generate_math_command(segment, state, MATH_STORE);
    }
}

// Starting from the first memory position, move to a specific register,
// preserving the value in the math ring
static void move_to_reg(WhirlCodeSegment *segment, RingState *state, Reg reg) {
    generate_op_command(segment, state, OP_ONE);

    for (int i = 0; i < (int) reg + REG_START_POS; i++) {
        generate_op_command(segment, state, OP_DADD);
    }
}

// Starting from the given register, move back to the first memory position.
// preserving the value in the math ring. 
static void move_back_from_reg(WhirlCodeSegment *segment, RingState *state, Reg reg) {
    generate_op_command(segment, state, OP_ONE);

    for (int i = reg + REG_START_POS; i < EXTRA_TMP1; i++) {
        generate_op_command(segment, state, OP_DADD);
    }

    generate_math_command(segment, state, MATH_STORE);
    generate_op_command(segment, state, OP_DADD);
    generate_op_command(segment, state, OP_STORE);
    generate_math_command(segment, state, MATH_LOAD);
    generate_math_command(segment, state, MATH_NEG);
    generate_math_command(segment, state, MATH_STORE);
    generate_op_command(segment, state, OP_LOAD);
    generate_op_command(segment, state, OP_DADD);
    generate_math_command(segment, state, MATH_LOAD);

    for (int i = 0; i < EXTRA_TMP1; i++) {
        generate_op_command(segment, state, OP_DADD);
    }
}

static WhirlCodeSegment *generate_data_initialization(RingState *state, const Data *data) {
    WhirlCodeSegment *segment = new_segment(NULL, NULL);

    if (data == NULL) {
        return segment;
    }

    set_mem(segment, state, DATA_START);
    generate_op_command(segment, state, OP_LOAD);
    generate_op_command(segment, state, OP_DADD);
    generate_op_command(segment, state, OP_ONE);

    int mem_pos = DATA_START;

    while (data != NULL) {
        generate_op_command(segment, state, OP_DADD);
        set_mem(segment, state, data->v);
        generate_op_command(segment, state, OP_DADD);
        data = data->next;
        mem_pos += 2;
    }

    set_mem(segment, state, -mem_pos);
    generate_op_command(segment, state, OP_LOAD);
    generate_op_command(segment, state, OP_DADD);

    reset_to_after_jump(segment, state);

    return segment;
}

// Make it so the math ring is set to be clockwise, is set to MATH_NOOP, and switch
// to the operations ring
static void set_math_ring_clockwise(WhirlCodeSegment *segment, RingState *state) {
    if (state->math_dir == CLOCKWISE && state->cur_math_pos == MATH_NOOP) {
        return;
    }

    if (state->active_ring == OPERATION_RING) {
        generate_op_command(segment, state, OP_NOOP);
    }

    if (state->math_dir != CLOCKWISE) {
        if (state->cur_math_pos == MATH_NOOP) {
            emit_one(segment, state);
        }
        emit_zero(segment, state);
    }

    while (state->cur_math_pos != MATH_NOOP) {
        emit_one(segment, state);
    }

    emit_zero(segment, state);
    emit_zero(segment, state);
}

// Execute a command on the operation ring in a way such that the op
// ring will be clockwise afterward. This expects that the current ring
// is the operations ring
static void do_op_command_clockwise(WhirlCodeSegment *segment, RingState *state, OpCmd cmd) {
    assert(state->active_ring == OPERATION_RING);

    if (state->op_dir != CLOCKWISE) {
        if (state->cur_op_pos == cmd) {
            emit_one(segment, state);
        }
        emit_zero(segment, state);
    }

    while (state->cur_op_pos != cmd) {
        emit_one(segment, state);
    }

    emit_zero(segment, state);
    emit_zero(segment, state);
}

static void generate_comparison(WhirlCodeSegment *segment, RingState *state) {
    Inst *inst = segment->inst;

    if (inst->src.type == REG) {
        move_to_reg(segment, state, inst->src.reg);
        generate_math_command(segment, state, MATH_LOAD);
        move_back_from_reg(segment, state, inst->src.reg);
    }
    else {
        set_mem(segment, state, inst->src.imm);
    }

    move_to_reg(segment, state, inst->dst.reg);

    switch (inst->op) {
        case EQ: case NE:
            generate_math_command(segment, state, MATH_EQUAL);
            break;
        case LT: case GE:
            generate_math_command(segment, state, MATH_GREATER);
            break;
        case GT: case LE:
            generate_math_command(segment, state, MATH_LESS);
            break;
        default:
            // This function should only be called with a comparison function
            assert(false);
            break;
    }

    if (inst->op == NE || inst->op == GE || inst->op == LE) {
        generate_math_command(segment, state, MATH_NOT);
    }

    generate_math_command(segment, state, MATH_STORE);
    move_back_from_reg(segment, state, inst->dst.reg);
}

// Generates the code for an ELVM jump instruction. We don't actually jump to
// our final destination here, we jump to the end of the program where there is
// a jump table that we will use
static void generate_jump_inst(WhirlCodeSegment *segment, RingState *state) {
    Inst *inst = segment->inst;

    // Put our jump destination in the PC register so that when we
    // reach the jump table, we know where to jump to
    if (inst->jmp.type == REG) {
        move_to_reg(segment, state, inst->jmp.reg);
        generate_math_command(segment, state, MATH_LOAD);
        move_back_from_reg(segment, state, inst->jmp.reg);
        move_to_reg(segment, state, PC_REG_NUM);
        generate_math_command(segment, state, MATH_STORE);
        move_back_from_reg(segment, state, PC_REG_NUM);
    }
    else {
        move_to_reg(segment, state, PC_REG_NUM);
        set_mem(segment, state, inst->jmp.imm);
        move_back_from_reg(segment, state, PC_REG_NUM);
    }

    // Set the current memory position to the location of the jump table,
    // which should be at the end of the program
    if (segment->next != NULL) {
        set_mem(segment, state, segment->next->total_code_len + 1);
    }
    else {
        set_mem(segment, state, 1);
    }

    // Unconditional jump, just load the address and store a 1 in the cell
    if (inst->op == JMP) {
        generate_op_command(segment, state, OP_LOAD);
        generate_math_command(segment, state, MATH_LOAD);
        generate_math_command(segment, state, MATH_EQUAL);
        generate_math_command(segment, state, MATH_STORE);
    }
    // Otherwise, we have to do an actual comparison and save the result in
    // the first memory cell
    else {
        generate_op_command(segment, state, OP_ONE);

        if (inst->src.type == REG) {
            for (unsigned i = 0; i < inst->src.reg + REG_START_POS; i++) {
                generate_op_command(segment, state, OP_DADD);
            }
            generate_math_command(segment, state, MATH_LOAD);
            for (int i = inst->src.reg + REG_START_POS; i < EXTRA_TMP1; i++) {
                generate_op_command(segment, state, OP_DADD);
            }
            generate_math_command(segment, state, MATH_STORE);
        }
        else {
            for (int i = 0; i < EXTRA_TMP1; i++) {
                generate_op_command(segment, state, OP_DADD);
            }
            set_mem(segment, state, inst->src.imm);
        }
        generate_op_command(segment, state, OP_DADD);
        generate_op_command(segment, state, OP_STORE);
        generate_math_command(segment, state, MATH_LOAD);
        generate_math_command(segment, state, MATH_NEG);
        generate_math_command(segment, state, MATH_STORE);
        generate_op_command(segment, state, OP_LOAD);
        generate_op_command(segment, state, OP_DADD);
        generate_math_command(segment, state, MATH_LOAD);

        for (unsigned i = EXTRA_TMP1; i > inst->dst.reg + REG_START_POS; i--) {
            generate_op_command(segment, state, OP_DADD);
        }

        switch (inst->op) {
            case JEQ: case JNE:
                generate_math_command(segment, state, MATH_EQUAL);
                break;
            case JLT: case JGE:
                generate_math_command(segment, state, MATH_GREATER);
                break;
            case JGT: case JLE:
                generate_math_command(segment, state, MATH_LESS);
                break;
            default:
                assert(false);
                break;
        }

        if (inst->op == JNE || inst->op == JGE || inst->op == JLE) {
            generate_math_command(segment, state, MATH_NOT);
        }

        for (int i = inst->dst.reg + REG_START_POS; i > 0; i--) {
            generate_op_command(segment, state, OP_DADD);
        }

        generate_op_command(segment, state, OP_LOAD);
        generate_math_command(segment, state, MATH_STORE);
    }

    // These last two commands do the jump, and set the rings clockwise. The rings
    // must be clockwise, because the jump table code assumes that the rings will
    // be clockwise, and it'll execute the wrong instructions otherwise
    set_math_ring_clockwise(segment, state);
    do_op_command_clockwise(segment, state, OP_IF);
}

// Generates an elvm ADD instruction
static void generate_add(WhirlCodeSegment *segment, RingState *state) {
    Inst *inst = segment->inst;
    
    if (inst->src.type == REG) {
        // Get the value from the register
        move_to_reg(segment, state, inst->src.reg);
        generate_math_command(segment, state, MATH_LOAD);
        move_back_from_reg(segment, state, inst->src.reg);

        // Add it to the destination register
        move_to_reg(segment, state, inst->dst.reg);
        generate_math_command(segment, state, MATH_ADD);
        generate_math_command(segment, state, MATH_STORE);
    }
    else {
        // Get the value from destination register, save it in the operation ring
        move_to_reg(segment, state, inst->dst.reg);
        generate_op_command(segment, state, OP_LOAD);

        // Load the immediate value into the math ring
        set_mem(segment, state, inst->src.imm);

        // Put the saved value back into the memory cell, add the immediate to it
        generate_op_command(segment, state, OP_STORE);
        generate_math_command(segment, state, MATH_ADD);
        generate_math_command(segment, state, MATH_STORE);
    }

    // Everything that follows in this function is to implement 24-bit arithmetic
    // while avoiding any jump instructions and accounting for the fact that Whirl
    // does not provide a modulo function

    // Move to a temp cell, and move the sum value to the operation ring
    move_back_from_reg(segment, state, inst->dst.reg);
    generate_math_command(segment, state, MATH_STORE);
    generate_op_command(segment, state, OP_LOAD);

    // Compare the sum value to the max limit, and set the math value to
    // negative -1 if it's greater, or 0 if it's not
    set_mem(segment, state, UINT24_LIMIT);
    generate_op_command(segment, state, OP_STORE);
    generate_math_command(segment, state, MATH_GREATER);
    generate_math_command(segment, state, MATH_NOT);
    generate_math_command(segment, state, MATH_STORE);
    generate_op_command(segment, state, OP_LOAD);

    // Multiply the math value by the 1 << 24, so if the sum value was greater
    // than 2^24-1, it will be set to negative 1 << 24, otherwise it'll be 0
    set_mem(segment, state, UINT24_LIMIT);
    generate_op_command(segment, state, OP_STORE);
    generate_math_command(segment, state, MATH_NEG);
    generate_math_command(segment, state, MATH_MULT);

    // Move back to the register, and add the math value to it, so if the addition
    // was equal to or greater than the limit, it will be properly modulo-d
    move_to_reg(segment, state, inst->dst.reg);
    generate_math_command(segment, state, MATH_ADD);
    generate_math_command(segment, state, MATH_STORE);
    move_back_from_reg(segment, state, inst->dst.reg);
}

// Generates an elvm SUB instruction
static void generate_sub(WhirlCodeSegment *segment, RingState *state) {
    Inst *inst = segment->inst;

    if (inst->src.type == REG) {
        // Get the value from the source register, and negate it
        move_to_reg(segment, state, inst->src.reg);
        generate_math_command(segment, state, MATH_LOAD);
        generate_math_command(segment, state, MATH_NEG);
        move_back_from_reg(segment, state, inst->src.reg);

        // Add the negated version to the destination register
        move_to_reg(segment, state, inst->dst.reg);
        generate_math_command(segment, state, MATH_ADD);
        generate_math_command(segment, state, MATH_STORE);
    }
    else {
        // Move to the destination register, and save the value in the register
        // in the operations ring
        move_to_reg(segment, state, inst->dst.reg);
        generate_op_command(segment, state, OP_LOAD);

        // Set the memory in the cell to the negated immediate value
        set_mem(segment, state, -inst->src.imm);

        // Add the negated immediate value to the saved value in the operation
        // ring, and save it to the register
        generate_op_command(segment, state, OP_STORE);
        generate_math_command(segment, state, MATH_ADD);
        generate_math_command(segment, state, MATH_STORE);
    }

    move_back_from_reg(segment, state, inst->dst.reg);

    // Everything that follows is used to implement 24-bit arithmetic, so that
    // when a number goes negative, it will wrap around appropriately. This is
    // written in a way to avoid jump instructions

    // Check to see if the subtracted value is negative, and store 1 in the
    // operation ring if so, or 0 otherwise
    generate_op_command(segment, state, OP_ZERO);
    generate_op_command(segment, state, OP_STORE);
    generate_math_command(segment, state, MATH_LESS);
    generate_math_command(segment, state, MATH_STORE);
    generate_op_command(segment, state, OP_LOAD);

    // Multiply (1 << 24) by the value in the operations ring, and put it into
    // the math value
    set_mem(segment, state, UINT24_LIMIT);
    generate_op_command(segment, state, OP_STORE);
    generate_math_command(segment, state, MATH_MULT);

    // Add the value in the math ring to the destination register, so if the
    // value is less than zero, then it will have (1 << 24) added to it, otherwise
    // it will have zero added to it
    move_to_reg(segment, state, inst->dst.reg);
    generate_math_command(segment, state, MATH_ADD);
    generate_math_command(segment, state, MATH_STORE);
    move_back_from_reg(segment, state, inst->dst.reg);
}

// Generates whirl code implementing a elvm LOAD instruction
static void generate_load(WhirlCodeSegment *segment, RingState *state) {
    Inst *inst = segment->inst;

    if (inst->src.type == REG) {
        // Get the jump address from the register, and convert it to the actual
        // memory position by multiplying it by two and adding the offset of the
        // data to it
        set_mem(segment, state, DATA_START);
        move_to_reg(segment, state, inst->src.reg);
        generate_math_command(segment, state, MATH_LOAD);
        generate_math_command(segment, state, MATH_ADD);
        move_back_from_reg(segment, state, inst->src.reg);
        generate_math_command(segment, state, MATH_ADD);
        generate_math_command(segment, state, MATH_STORE);
    }
    else {
        // Set up the current memory position to have the correct address for the
        // memory we're loading from
        set_mem(segment, state, inst->src.imm * 2 + DATA_START);
    }

    // Load the memory address, jump to it
    generate_op_command(segment, state, OP_LOAD);
    generate_op_command(segment, state, OP_DADD);

    // Save the memory address in the current cell, but negated, so we know
    // how far back in memory to jump to
    generate_op_command(segment, state, OP_STORE);
    generate_math_command(segment, state, MATH_LOAD);
    generate_math_command(segment, state, MATH_NEG);
    generate_math_command(segment, state, MATH_STORE);

    // Move to the temp cell to the right of the memory we want to load
    generate_op_command(segment, state, OP_ONE);
    generate_op_command(segment, state, OP_DADD);
    generate_op_command(segment, state, OP_DADD);

    // Now we set operation value to negative one so we can go backwards in
    // memory
    generate_math_command(segment, state, MATH_LOAD);
    generate_math_command(segment, state, MATH_EQUAL);
    generate_math_command(segment, state, MATH_NEG);
    generate_math_command(segment, state, MATH_STORE);
    generate_op_command(segment, state, OP_LOAD);

    // Move back to the memory we want to load, and load it into the math value
    generate_op_command(segment, state, OP_DADD);
    generate_math_command(segment, state, MATH_LOAD);

    // Now go back to the position where we saved the memory address
    generate_op_command(segment, state, OP_DADD);
    generate_op_command(segment, state, OP_LOAD);
    generate_op_command(segment, state, OP_DADD);

    // Save the math value in the destination register
    move_to_reg(segment, state, inst->dst.reg);
    generate_math_command(segment, state, MATH_STORE);
    move_back_from_reg(segment, state, inst->dst.reg);
}

// Generates whirl code implementing a elvm STORE instruction
static void generate_store(WhirlCodeSegment *segment, RingState *state) {
    Inst *inst = segment->inst;

    if (inst->src.type == REG) {
        // Get the jump address from the register, and convert it to the actual
        // memory position by multiplying it by two and adding the offset of the
        // data to it
        set_mem(segment, state, DATA_START);
        move_to_reg(segment, state, inst->src.reg);
        generate_math_command(segment, state, MATH_LOAD);
        generate_math_command(segment, state, MATH_ADD);
        move_back_from_reg(segment, state, inst->src.reg);
        generate_math_command(segment, state, MATH_ADD);
        generate_math_command(segment, state, MATH_STORE);
    }
    else {
        // Set up the current memory position to have the correct address for the
        // memory we're loading from
        set_mem(segment, state, inst->src.imm * 2 + DATA_START);
    }

    // Load the value from the dest register into the math ring
    move_to_reg(segment, state, inst->dst.reg);
    generate_math_command(segment, state, MATH_LOAD);
    move_back_from_reg(segment, state, inst->dst.reg);

    // Jump to the memory address, and save the jump amount in this temp cell,
    // so we know how far to jump back
    generate_op_command(segment, state, OP_LOAD);
    generate_op_command(segment, state, OP_DADD);
    generate_op_command(segment, state, OP_STORE);

    // Move to the memory position we want, and store the math value in it
    generate_op_command(segment, state, OP_ONE);
    generate_op_command(segment, state, OP_DADD);
    generate_math_command(segment, state, MATH_STORE);

    // Move over to the next memory position, and store a negative one in the
    // operation ring
    generate_op_command(segment, state, OP_DADD);
    generate_op_command(segment, state, OP_STORE);
    generate_math_command(segment, state, MATH_LOAD);
    generate_math_command(segment, state, MATH_NEG);
    generate_math_command(segment, state, MATH_STORE);
    generate_op_command(segment, state, OP_LOAD);

    // Move to the memory position with the address offset, negate it, and then
    // use it to jump back to the first position in memory
    generate_op_command(segment, state, OP_DADD);
    generate_op_command(segment, state, OP_DADD);
    generate_math_command(segment, state, MATH_LOAD);
    generate_math_command(segment, state, MATH_NEG);
    generate_math_command(segment, state, MATH_STORE);
    generate_op_command(segment, state, OP_LOAD);
    generate_op_command(segment, state, OP_DADD);
}

static void generate_segment(WhirlCodeSegment *segment, RingState *state) {
    const Inst *inst = segment->inst;

    switch (inst->op) {
        case EQ: case NE:
        case LE: case LT:
        case GE: case GT:
            generate_comparison(segment, state);
            break;

        case JEQ: case JNE:
        case JLE: case JLT:
        case JGE: case JGT:
        case JMP:
            generate_jump_inst(segment, state);
            break;

        case MOV:
            if (inst->src.type == REG) {
                move_to_reg(segment, state, inst->src.reg);
                generate_math_command(segment, state, MATH_LOAD);
                move_back_from_reg(segment, state, inst->src.reg);
                move_to_reg(segment, state, inst->dst.reg);
                generate_math_command(segment, state, MATH_STORE);
                move_back_from_reg(segment, state, inst->dst.reg);
            }
            else {
                move_to_reg(segment, state, inst->dst.reg);
                set_mem(segment, state, inst->src.imm);
                move_back_from_reg(segment, state, inst->dst.reg);
            }
            break;

        case ADD:
            generate_add(segment, state);
            break;

        case SUB:
            generate_sub(segment, state);
            break;

        case GETC:
            move_to_reg(segment, state, inst->dst.reg);
            generate_op_command(segment, state, OP_ZERO);
            generate_op_command(segment, state, OP_ASC_IO);

            // Check if it's negative one. ELVM expects EOF to be zero, so
            // add one to the stored input if it's negative
            generate_math_command(segment, state, MATH_ZERO);
            generate_math_command(segment, state, MATH_GREATER);
            generate_math_command(segment, state, MATH_ADD);
            generate_math_command(segment, state, MATH_STORE);
            move_back_from_reg(segment, state, inst->dst.reg);
            break;

        case PUTC:
            if (inst->src.type == IMM) {
                set_mem(segment, state, inst->src.imm);
                generate_op_command(segment, state, OP_ONE);
                generate_op_command(segment, state, OP_ASC_IO);
            }
            else {
                move_to_reg(segment, state, inst->src.reg);
                generate_op_command(segment, state, OP_ONE);
                generate_op_command(segment, state, OP_ASC_IO);
                move_back_from_reg(segment, state, inst->src.reg);
            }
            break;

        case LOAD:
            generate_load(segment, state);
            break;

        case STORE:
            generate_store(segment, state);
            break;

        case EXIT:
            generate_op_command(segment, state, OP_EXIT);
            break;

        case DUMP:
        default:
            // Do nothing
            break;
    }
}

static bool is_jump(Op op) {
    const Op JUMP_OPS[] = {
        JMP, JEQ, JNE, JLT, JLE, JGT, JGE,
    };
    for (int i = 0; i < 7; i++) {
        if (op == JUMP_OPS[i]) {
            return true;
        }
    }
    return false;
}

// Goes through the code segments in reverse order, generating the Whirl code to
// implement each ELVM instruction. This must be done in reverse order, because
// jump instructions need to know how far forward they have to jump, so all of the
// later instructions must be generated already. 
static void generate_code_segments(WhirlCodeSegment *last_segment, RingState *state) {
    WhirlCodeSegment *first_segment = last_segment;

    while (first_segment != NULL) {
        last_segment = first_segment;
        if (is_jump(first_segment->inst->op)) {
            generate_segment(first_segment, state);
        }
        else {
            // We want to minimize the amount of times we reset the ring states,
            // so we try to generate the longest string of instructions at once
            // that we can. We can only generate the same instructions at once if
            // they have same PC and they don't have any jump instructions
            while (first_segment->prev != NULL &&
                   !is_jump(first_segment->prev->inst->op) &&
                   first_segment->prev->inst->pc == first_segment->inst->pc)
            {
                first_segment = first_segment->prev;
            }

            WhirlCodeSegment *segment = first_segment;
            while (segment != last_segment->next) {
                generate_segment(segment, state);
                segment = segment->next;
            }
            reset_to_after_jump(last_segment, state);
        }

        // Update the total code length field for the newly-added code segments
        for (WhirlCodeSegment *segment = last_segment; segment != first_segment->prev; segment = segment->prev) {
            if (segment->next == NULL) {
                segment->total_code_len = segment->len;
            }
            else {
                segment->total_code_len = segment->len + segment->next->total_code_len;
            }
        }

        first_segment = first_segment->prev;
    }
}

// Generates a jump instruction for the end of program jump table. Given the length that
// the jump segment must be, and the amount backwards it has to jump at the end, it will
// return a segment that will perform that jump, or a null pointer if it was unable to
// create a segment in the given size
static WhirlCodeSegment *generate_jump_for_table(WhirlCodeSegment *last_segment, RingState state, int jump_amount, unsigned length) {
    RingState start_state = state;
    WhirlCodeSegment *segment = NULL;

    // We may not end up being able to create a jump aligned how we would like inside the
    // length given to us, so we do a loop here to try out different lengths, and then pad
    // them out to match the length we're trying for
    for (int i = 0; i < 200; i++) {
        size_t actual_len = length - i;
        int actual_jump = jump_amount - i;
        state = start_state;

        segment = new_segment(NULL, last_segment);
        set_mem(segment, &state, -actual_jump);
        generate_op_command(segment, &state, OP_LOAD);
        set_math_ring_clockwise(segment, &state);

        if (state.op_dir != CLOCKWISE) {
            emit_zero(segment, &state);
            emit_one(segment, &state);
        }

        while (segment->len < actual_len &&
            (actual_len - segment->len + state.cur_op_pos - 2) % NUM_CMDS_PER_RING != OP_IF)
        {
            emit_zero(segment, &state);
            emit_one(segment, &state);
            emit_one(segment, &state);
            emit_zero(segment, &state);
            emit_one(segment, &state);
        }

        // We've exceeded the length without aligning with the OP_IF command,
        // so we give up on this length and try again
        if (segment->len >= actual_len) {
            free_segments(segment);
            segment = NULL;
            continue;
        }

        while (segment->len < actual_len - 2) {
            emit_one(segment, &state);
        }

        emit_zero(segment, &state);
        emit_zero(segment, &state);

        for (int j = 0; j < i; j++) {
            emit_zero(segment, &state);
        }

        return segment;
    }

    return segment;
}

// Generates the jump table at the end of the program. Since the program will be
// completely generated by this point, we will know how far to jump backwards in
// the code for each PC value. 
static WhirlCodeSegment *generate_jump_table(WhirlCodeSegment *first_segment) {
    WhirlCodeSegment *table_start = new_segment(NULL, NULL);
    int table_segment_size = 300;
    bool made_table = false;

    while (!made_table) {
        RingState *state = &(RingState) {
            .active_ring  = MATH_RING,
            .cur_op_pos   = OP_IF,
            .cur_math_pos = MATH_NOOP,
            .op_dir       = CLOCKWISE,
            .math_dir     = CLOCKWISE,
            .last_is_zero = false,
        };

        // The jump table consists of a list of code segments which jump to the
        // appropriate piece of code for each PC value. Each code segment is a
        // fixed size, so to jump to the correct segment we multiply the PC value
        // by that fixed size to get the correct jump amount.
        set_mem(table_start, state, table_segment_size);
        move_to_reg(table_start, state, PC_REG_NUM);
        generate_math_command(table_start, state, MATH_LOAD);
        move_back_from_reg(table_start, state, PC_REG_NUM);
        generate_math_command(table_start, state, MATH_MULT);

        // We need to add 1 to the jump amount to the get the correct jump amount
        generate_math_command(table_start, state, MATH_STORE);
        generate_math_command(table_start, state, MATH_EQUAL);
        generate_math_command(table_start, state, MATH_ADD);
        generate_math_command(table_start, state, MATH_STORE);
        generate_op_command(table_start, state, OP_LOAD);

        // Jump to the correct segment implementing the jump we want
        generate_op_command(table_start, state, OP_PADD);

        RingState saved_jump_state = *state;
        int last_pc = -1;
        int num_jumps = 0;

        WhirlCodeSegment *last_table_segment = table_start;

        bool table_failed = false;

        for (WhirlCodeSegment *segment = first_segment; segment != NULL; segment = segment->next) {
            if (segment->inst->pc != last_pc) {
                // If we encounter a new PC value, we create a new segment
                // implementing the jump
                num_jumps++;

                int jump_amount = table_start->len + segment->total_code_len + table_segment_size * num_jumps - 1;
                WhirlCodeSegment *new_segment = generate_jump_for_table(last_table_segment, saved_jump_state, jump_amount, table_segment_size);

                if (new_segment == NULL) {
                    table_failed = true;
                    break;
                }

                last_table_segment = new_segment;
                last_pc = segment->inst->pc;
            }
        }

        // If we were unable to create the table, we increase the segment size
        // and try again.
        if (table_failed) {
            table_segment_size += 100;
            table_start = new_segment(NULL, NULL);
        }
        else {
            made_table = true;
        }
    }

    return table_start;
}

void target_whirl(Module *module) {
    RingState *ring_state = &(RingState){
        .active_ring  = OPERATION_RING,
        .cur_op_pos   = OP_NOOP,
        .cur_math_pos = MATH_NOOP,
        .op_dir       = CLOCKWISE,
        .math_dir     = CLOCKWISE,
        .last_is_zero = false
    };

    WhirlCodeSegment *head_segment = NULL;
    WhirlCodeSegment *cur_segment = NULL;

    for (Inst *inst = module->text; inst != NULL; inst = inst->next) {
        cur_segment = new_segment(inst, cur_segment);

        if (head_segment == NULL) {
            head_segment = cur_segment;
        }
    }

    Inst *exit_inst = &(Inst){
        .op = EXIT,
        .pc = cur_segment->inst->pc,
        .next = NULL,
    };

    cur_segment = new_segment(exit_inst, cur_segment);

    WhirlCodeSegment *data_init = generate_data_initialization(ring_state, module->data);

    generate_code_segments(cur_segment, ring_state);

    WhirlCodeSegment *jump_table = generate_jump_table(head_segment);

    data_init->next = head_segment;
    head_segment->prev = data_init;
    cur_segment->next = jump_table;
    jump_table->prev = cur_segment;

    output_segments(data_init);
    free_segments(head_segment);
}
