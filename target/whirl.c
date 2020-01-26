#include <ir/ir.h>
#include <target/util.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

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

typedef struct {
    WhirlRing active_ring;

    OpCmd cur_op_pos;

    MathCmd cur_math_pos;

    RingDirection op_dir;

    RingDirection math_dir;

    bool last_is_zero;
} RingState;

typedef struct CodeSegment_ {
    Inst *inst;

    char *code;

    size_t alloc, len;

    size_t total_code_len;

    struct CodeSegment_ *next, *prev;
} WhirlCodeSegment;

#define INIT_CODE_ALLOC 32

// Memory layout:
// TMP PC A B C D BP SP TMP TMP MEM0 TMP MEM1 TMP MEM2 ...
/*
static const int PC_POS     = 1;
*/
static const int REG_A_POS  = 1;
/*
static const int REG_B_POS  = 3;
static const int REG_C_POS  = 4;
static const int REG_D_POS  = 5;
static const int REG_BP_POS = 6;
static const int REG_SP_POS = 7;
*/
static const int PC_REG_NUM = 6;
static const int EXTRA_TMP1 = 8;
/*
static const int EXTRA_TMP2 = 9;
*/
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

static RingDirection reverse_direction(RingDirection dir) {
    return dir == CLOCKWISE ? COUNTERCLOCKWISE : CLOCKWISE;
}

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

static void generate_math_command(WhirlCodeSegment *segment, RingState *state, MathCmd cmd);

bool should_switch(int cur_pos, int target_pos, RingDirection dir) {
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

static void generate_op_command(WhirlCodeSegment *segment, RingState *state, OpCmd cmd) {
    if (state->active_ring == MATH_RING) {
        generate_math_command(segment, state, OP_NOOP);
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

static void generate_math_command(WhirlCodeSegment *segment, RingState *state, MathCmd cmd) {
    if (state->active_ring == OPERATION_RING) {
        generate_op_command(segment, state, MATH_NOOP);
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

    for (int i = 0; i < (int) reg + REG_A_POS; i++) {
        generate_op_command(segment, state, OP_DADD);
    }
}

// Starting from the given register, move back to the first memory position.
// preserving the value in the math ring. 
static void move_back_from_reg(WhirlCodeSegment *segment, RingState *state, Reg reg) {
    generate_op_command(segment, state, OP_ONE);

    for (int i = reg + REG_A_POS; i < EXTRA_TMP1; i++) {
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

static WhirlCodeSegment *generate_data_initialization(RingState *state, Data *data) {
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

static void generate_boolean_op(WhirlCodeSegment *segment, RingState *state) {
    Inst *inst = segment->inst;

    if (inst->src.type == REG) {
        move_to_reg(segment, state, inst->src.reg);
        generate_math_command(segment, state, MATH_LOAD);
        move_back_from_reg(segment, state, inst->src.reg);
    }
    else {
        set_mem(segment, state, inst->src.imm);
        generate_math_command(segment, state, MATH_LOAD);
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
            // This function should only be called with a boolean function
            assert(false);
            break;
    }

    if (inst->op == NE || inst->op == GE || inst->op == LE) {
        generate_math_command(segment, state, MATH_NOT);
    }

    generate_math_command(segment, state, MATH_STORE);
    move_back_from_reg(segment, state, inst->dst.reg);
}

static void generate_jump_inst(WhirlCodeSegment *segment, RingState *state) {
    Inst *inst = segment->inst;

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

    if (segment->next != NULL) {
        set_mem(segment, state, segment->next->total_code_len + 1);
    }
    else {
        set_mem(segment, state, 1);
    }

    if (inst->op == JMP) {
        // Unconditional jump, just load the address and store a 1 in the cell
        generate_op_command(segment, state, OP_LOAD);
        generate_math_command(segment, state, MATH_LOAD);
        generate_math_command(segment, state, MATH_EQUAL);
        generate_math_command(segment, state, MATH_STORE);
    }
    else {
        generate_op_command(segment, state, OP_ONE);

        // Otherwise, we have to do things the hard way
        if (inst->src.type == REG) {
            for (unsigned i = 0; i < inst->src.reg + REG_A_POS; i++) {
                generate_op_command(segment, state, OP_DADD);
            }
            generate_math_command(segment, state, MATH_LOAD);
            for (int i = inst->src.reg + REG_A_POS; i < EXTRA_TMP1; i++) {
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

        for (unsigned i = EXTRA_TMP1; i > inst->dst.reg + REG_A_POS; i--) {
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

        for (int i = inst->dst.reg + REG_A_POS; i > 0; i--) {
            generate_op_command(segment, state, OP_DADD);
        }

        generate_op_command(segment, state, OP_LOAD);
        generate_math_command(segment, state, MATH_STORE);
    }

    set_math_ring_clockwise(segment, state);
    do_op_command_clockwise(segment, state, OP_IF);
}

static void generate_segment(WhirlCodeSegment *segment, RingState *state) {
    const Inst *inst = segment->inst;

    switch (inst->op) {
        case EQ: case NE:
        case LE: case LT:
        case GE: case GT:
            generate_boolean_op(segment, state);
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
            if (inst->src.type == REG) {
                move_to_reg(segment, state, inst->src.reg);
                generate_math_command(segment, state, MATH_LOAD);
                move_back_from_reg(segment, state, inst->src.reg);
                move_to_reg(segment, state, inst->dst.reg);
                generate_math_command(segment, state, MATH_ADD);
                generate_math_command(segment, state, MATH_STORE);
                move_back_from_reg(segment, state, inst->dst.reg);
            }
            else {
                move_to_reg(segment, state, inst->dst.reg);
                generate_op_command(segment, state, OP_LOAD);
                set_mem(segment, state, inst->src.imm);
                generate_math_command(segment, state, MATH_LOAD);
                generate_op_command(segment, state, OP_STORE);
                generate_math_command(segment, state, MATH_ADD);
                generate_math_command(segment, state, MATH_STORE);
                move_back_from_reg(segment, state, inst->dst.reg);
            }
            break;

        case SUB:
            if (inst->src.type == REG) {
                move_to_reg(segment, state, inst->src.reg);
                generate_math_command(segment, state, MATH_LOAD);
                generate_math_command(segment, state, MATH_NEG);
                move_back_from_reg(segment, state, inst->src.reg);
                move_to_reg(segment, state, inst->dst.reg);
                generate_math_command(segment, state, MATH_ADD);
                generate_math_command(segment, state, MATH_STORE);
                move_back_from_reg(segment, state, inst->dst.reg);
            }
            else {
                move_to_reg(segment, state, inst->dst.reg);
                generate_op_command(segment, state, OP_LOAD);
                set_mem(segment, state, inst->src.imm);
                generate_math_command(segment, state, MATH_LOAD);
                generate_math_command(segment, state, MATH_NEG);
                generate_op_command(segment, state, OP_STORE);
                generate_math_command(segment, state, MATH_ADD);
                generate_math_command(segment, state, MATH_STORE);
                move_back_from_reg(segment, state, inst->dst.reg);
            }
            break;

        case GETC:
            move_to_reg(segment, state, inst->dst.reg);
            generate_op_command(segment, state, OP_ZERO);
            generate_op_command(segment, state, OP_ASC_IO);
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
            if (inst->src.type == REG) {
                set_mem(segment, state, DATA_START);
                move_to_reg(segment, state, inst->src.reg);
                generate_math_command(segment, state, MATH_LOAD);
                generate_math_command(segment, state, MATH_ADD);
                move_back_from_reg(segment, state, inst->src.reg);
                generate_math_command(segment, state, MATH_ADD);
                generate_math_command(segment, state, MATH_STORE);
            }
            else {
                set_mem(segment, state, inst->src.imm * 2 + DATA_START);
            }
            generate_op_command(segment, state, OP_LOAD);
            generate_op_command(segment, state, OP_DADD);
            generate_op_command(segment, state, OP_STORE);
            generate_math_command(segment, state, MATH_LOAD);
            generate_math_command(segment, state, MATH_NEG);
            generate_math_command(segment, state, MATH_STORE);
            generate_op_command(segment, state, OP_ONE);
            generate_op_command(segment, state, OP_DADD);
            generate_math_command(segment, state, MATH_LOAD);
            generate_op_command(segment, state, OP_DADD);
            generate_math_command(segment, state, MATH_STORE);
            generate_op_command(segment, state, OP_DADD);            
            generate_op_command(segment, state, OP_DADD);
            generate_op_command(segment, state, OP_STORE);
            generate_math_command(segment, state, MATH_LOAD);
            generate_math_command(segment, state, MATH_NEG);
            generate_math_command(segment, state, MATH_STORE);
            generate_op_command(segment, state, OP_LOAD);
            generate_op_command(segment, state, OP_DADD);
            generate_op_command(segment, state, OP_DADD);
            generate_math_command(segment, state, MATH_LOAD);
            generate_op_command(segment, state, OP_DADD);
            generate_op_command(segment, state, OP_DADD);
            generate_op_command(segment, state, OP_LOAD);
            generate_op_command(segment, state, OP_DADD);
            move_to_reg(segment, state, inst->dst.reg);
            generate_math_command(segment, state, MATH_STORE);
            move_back_from_reg(segment, state, inst->dst.reg);
            break;

        case STORE:
            if (inst->src.type == REG) {
                set_mem(segment, state, DATA_START);
                move_to_reg(segment, state, inst->src.reg);
                generate_math_command(segment, state, MATH_LOAD);
                generate_math_command(segment, state, MATH_ADD);
                move_back_from_reg(segment, state, inst->src.reg);
                generate_math_command(segment, state, MATH_ADD);
                generate_math_command(segment, state, MATH_STORE);
            }
            else {
                set_mem(segment, state, inst->src.imm * 2 + DATA_START);
            }
            move_to_reg(segment, state, inst->dst.reg);
            generate_math_command(segment, state, MATH_LOAD);
            move_back_from_reg(segment, state, inst->dst.reg);
            generate_op_command(segment, state, OP_LOAD);
            generate_op_command(segment, state, OP_DADD);
            generate_op_command(segment, state, OP_STORE);
            generate_op_command(segment, state, OP_ONE);
            generate_op_command(segment, state, OP_DADD);
            generate_math_command(segment, state, MATH_STORE);
            generate_op_command(segment, state, OP_DADD);
            generate_op_command(segment, state, OP_STORE);
            generate_math_command(segment, state, MATH_LOAD);
            generate_math_command(segment, state, MATH_NEG);
            generate_math_command(segment, state, MATH_STORE);
            generate_op_command(segment, state, OP_LOAD);
            generate_op_command(segment, state, OP_DADD);
            generate_op_command(segment, state, OP_DADD);
            generate_math_command(segment, state, MATH_LOAD);
            generate_math_command(segment, state, MATH_NEG);
            generate_math_command(segment, state, MATH_STORE);
            generate_op_command(segment, state, OP_LOAD);
            generate_op_command(segment, state, OP_DADD);
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

static void generate_code_segments(WhirlCodeSegment *last_segment, RingState *state) {
    WhirlCodeSegment *first_segment = last_segment;

    while (first_segment != NULL) {
        last_segment = first_segment;
        if (is_jump(first_segment->inst->op)) {
            generate_segment(first_segment, state);
        }
        else {
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

static WhirlCodeSegment *generate_jump_for_table(WhirlCodeSegment *last_segment, RingState state, int jump_amount, unsigned length) {
    WhirlCodeSegment *segment = new_segment(NULL, last_segment);
    set_mem(segment, &state, -jump_amount);
    generate_op_command(segment, &state, OP_LOAD);
    set_math_ring_clockwise(segment, &state);

    if (state.op_dir != CLOCKWISE) {
        emit_zero(segment, &state);
        emit_one(segment, &state);
    }

    while (segment->len < length &&
           (length - segment->len + state.cur_op_pos - 2) % NUM_CMDS_PER_RING != OP_IF)
    {
        emit_zero(segment, &state);
        emit_one(segment, &state);
        emit_one(segment, &state);
        emit_zero(segment, &state);
        emit_one(segment, &state);
    }

    if (segment->len >= length) {
        return NULL;
    }

    while (segment->len < length - 2) {
        emit_one(segment, &state);
    }

    emit_zero(segment, &state);
    emit_zero(segment, &state);

    return segment;
}

static WhirlCodeSegment *generate_jump_table(WhirlCodeSegment *first_segment) {
    WhirlCodeSegment *table_start = new_segment(NULL, NULL);
    int table_segment_size = 500;
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

        set_mem(table_start, state, table_segment_size);
        move_to_reg(table_start, state, PC_REG_NUM);
        generate_math_command(table_start, state, MATH_LOAD);
        move_back_from_reg(table_start, state, PC_REG_NUM);
        generate_math_command(table_start, state, MATH_MULT);
        generate_math_command(table_start, state, MATH_STORE);
        generate_math_command(table_start, state, MATH_EQUAL);
        generate_math_command(table_start, state, MATH_ADD);
        generate_math_command(table_start, state, MATH_STORE);
        generate_op_command(table_start, state, OP_LOAD);
        generate_op_command(table_start, state, OP_PADD);

        RingState saved_jump_state = *state;
        int last_pc = -1;
        int num_jumps = 0;

        WhirlCodeSegment *last_table_segment = table_start;

        bool table_failed = false;

        for (WhirlCodeSegment *segment = first_segment; segment != NULL; segment = segment->next) {
            if (segment->inst->pc != last_pc) {
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

    // First thing: create a linked list of all the code segments
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
