#include <ir/ir.h>
#include <target/util.h>
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

    struct CodeSegment_ *next, *prev;
} WhirlCodeSegment;

#define INIT_CODE_ALLOC 32

// Memory layout:
// TMP PC A B C D BP SP TMP TMP MEM0 TMP MEM1 TMP MEM2 ...
/*
static const int PC_POS     = 1;
*/
static const int REG_A_POS  = 2;
/*
static const int REG_B_POS  = 3;
static const int REG_C_POS  = 4;
static const int REG_D_POS  = 5;
static const int REG_BP_POS = 6;
static const int REG_SP_POS = 7;
*/
static const int EXTRA_TMP1 = 8;
/*
static const int EXTRA_TMP2 = 9; 
static const int DATA_START = 10;
*/

static WhirlCodeSegment *new_segment(Inst *inst, WhirlCodeSegment *prev) {
    WhirlCodeSegment *segment = malloc(sizeof(WhirlCodeSegment));
    segment->inst = inst;
    segment->code = malloc(sizeof(char) * INIT_CODE_ALLOC);
    segment->alloc = INIT_CODE_ALLOC;
    segment->len = 0;
    segment->next = NULL;
    segment->prev = prev;
    if (prev != NULL) {
        prev->next = segment;
    }
    return segment;
}

static void output_segments(const WhirlCodeSegment *segment) {
    static const char* op_strs[] = {
        "mov", "add", "sub", "load", "store", "putc", "getc", "exit",
        "jeq", "jne", "jlt", "jgt", "jle", "jge", "jmp", "xxx",
        "eq", "ne", "lt", "gt", "le", "ge", "dump"
    };

    while (segment != NULL) {
        printf("%s: ", op_strs[segment->inst->op]);
        for (size_t i = 0; i < segment->len; i++) {
            putchar(segment->code[i]);
        }
        putchar('\n');

        segment = segment->next;
    }
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

static void generate_op_command(WhirlCodeSegment *segment, RingState *state, OpCmd cmd) {
    if (state->active_ring == MATH_RING) {
        generate_math_command(segment, state, OP_NOOP);
    }

    while (state->cur_op_pos != cmd) {
        emit_one(segment, state);
    }

    emit_zero(segment, state);
    emit_zero(segment, state);
}

static void reset_rings(WhirlCodeSegment *segment, RingState *state) {
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
        emit_zero(segment, state);
        emit_zero(segment, state);
    }

    if (state->active_ring == MATH_RING) {
        emit_zero(segment, state);
        emit_zero(segment, state);
    }

    if (state->op_dir != CLOCKWISE) {
        if (state->cur_op_pos == OP_NOOP) {
            emit_one(segment, state);
            emit_zero(segment, state);
            emit_one(segment, state);
        }
        else {
            emit_zero(segment, state);
        }
    }

    while (state->cur_op_pos != OP_NOOP) {
        emit_one(segment, state);
    }
}

static void generate_math_command(WhirlCodeSegment *segment, RingState *state, MathCmd cmd) {
    if (state->active_ring == OPERATION_RING) {
        generate_op_command(segment, state, MATH_NOOP);
    }

    while (state->cur_math_pos != cmd) {
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

    generate_math_command(segment, state, MATH_EQUAL);
    generate_math_command(segment, state, MATH_STORE);

    if (val == 1) {
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
}

// Starting from the first memory position, move to a specific memory position,
// preserving the value in the math ring
static void move_to_pos(WhirlCodeSegment *segment, RingState *state, int pos) {
    generate_op_command(segment, state, OP_ONE);

    for (int i = 0; i < pos; i++) {
        generate_op_command(segment, state, OP_DADD);
    }
}

// Starting from the given memory position, move back to a specific memory position,
// preserving the value in the math ring. Only works from the register positions
static void move_back_from_pos(WhirlCodeSegment *segment, RingState *state, int pos) {
    generate_op_command(segment, state, OP_ONE);

    for (int i = pos; i < EXTRA_TMP1; i++) {
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

static void generate_segment(WhirlCodeSegment *segment, RingState *state) {
    const Inst *inst = segment->inst;

    switch (inst->op) {
        case MOV:
            if (inst->src.type == REG) {
                move_to_pos(segment, state, inst->src.reg + REG_A_POS);
                generate_math_command(segment, state, MATH_LOAD);
                move_back_from_pos(segment, state, inst->src.reg + REG_A_POS);
                move_to_pos(segment, state, inst->dst.reg + REG_A_POS);
                generate_math_command(segment, state, MATH_STORE);
                move_back_from_pos(segment, state, inst->dst.reg + REG_A_POS);
            }
            else {
                move_to_pos(segment, state, inst->dst.reg + REG_A_POS);
                set_mem(segment, state, inst->src.imm);
                move_back_from_pos(segment, state, inst->dst.reg + REG_A_POS);
            }
            break;

        case ADD:
            if (inst->src.type == REG) {
                move_to_pos(segment, state, inst->src.reg + REG_A_POS);
                generate_math_command(segment, state, MATH_LOAD);
                move_back_from_pos(segment, state, inst->src.reg + REG_A_POS);
                move_to_pos(segment, state, inst->dst.reg + REG_A_POS);
                generate_math_command(segment, state, MATH_ADD);
                generate_math_command(segment, state, MATH_STORE);
                move_back_from_pos(segment, state, inst->dst.reg + REG_A_POS);
            }
            else {
                move_to_pos(segment, state, inst->dst.reg + REG_A_POS);
                generate_op_command(segment, state, OP_LOAD);
                set_mem(segment, state, inst->src.imm);
                generate_math_command(segment, state, MATH_LOAD);
                generate_op_command(segment, state, OP_STORE);
                generate_math_command(segment, state, MATH_ADD);
                generate_math_command(segment, state, MATH_STORE);
                move_back_from_pos(segment, state, inst->dst.reg + REG_A_POS);
            }
            break;

        case SUB:
            if (inst->src.type == REG) {
                move_to_pos(segment, state, inst->src.reg + REG_A_POS);
                generate_math_command(segment, state, MATH_LOAD);
                generate_math_command(segment, state, MATH_NEG);
                move_back_from_pos(segment, state, inst->src.reg + REG_A_POS);
                move_to_pos(segment, state, inst->dst.reg + REG_A_POS);
                generate_math_command(segment, state, MATH_ADD);
                generate_math_command(segment, state, MATH_STORE);
                move_back_from_pos(segment, state, inst->dst.reg + REG_A_POS);
            }
            else {
                move_to_pos(segment, state, inst->dst.reg + REG_A_POS);
                generate_op_command(segment, state, OP_LOAD);
                set_mem(segment, state, inst->src.imm);
                generate_math_command(segment, state, MATH_LOAD);
                generate_math_command(segment, state, MATH_NEG);
                generate_op_command(segment, state, OP_STORE);
                generate_math_command(segment, state, MATH_ADD);
                generate_math_command(segment, state, MATH_STORE);
                move_back_from_pos(segment, state, inst->dst.reg + REG_A_POS);
            }
            break;

        case PUTC:
            if (inst->src.type == IMM) {
                set_mem(segment, state, inst->src.imm);
                generate_op_command(segment, state, OP_ONE);
                generate_op_command(segment, state, OP_ASC_IO);
            }
            else {
                move_to_pos(segment, state, inst->src.reg + REG_A_POS);
                generate_op_command(segment, state, OP_ONE);
                generate_op_command(segment, state, OP_ASC_IO);
                move_back_from_pos(segment, state, inst->src.reg + REG_A_POS);
            }
            break;

        case EQ:
            if (inst->src.type == REG) {
                move_to_pos(segment, state, inst->src.reg + REG_A_POS);
                generate_math_command(segment, state, MATH_STORE);
                move_back_from_pos(segment, state, inst->src.reg + REG_A_POS);
                move_to_pos(segment, state, inst->dst.reg + REG_A_POS);
                generate_math_command(segment, state, MATH_EQUAL);
                generate_math_command(segment, state, MATH_STORE);
                move_back_from_pos(segment, state, inst->dst.reg + REG_A_POS);
            }
            else {
                set_mem(segment, state, inst->src.imm);
                generate_math_command(segment, state, MATH_LOAD);
                move_to_pos(segment, state, inst->dst.reg + REG_A_POS);
                generate_math_command(segment, state, MATH_EQUAL);
                generate_math_command(segment, state, MATH_STORE);
                move_back_from_pos(segment, state, inst->dst.reg + REG_A_POS);
            }
            break;

        case EXIT:
            generate_op_command(segment, state, OP_EXIT);
            break;

        case DUMP:
        default:
            // Do nothing
            break;
    }

    reset_rings(segment, state);
}

void target_whirl(Module *module) {
    RingState *ring_state = &(RingState){
        .active_ring = OPERATION_RING,
        .cur_op_pos = OP_NOOP,
        .cur_math_pos = MATH_NOOP,
        .op_dir = CLOCKWISE,
        .math_dir = CLOCKWISE,
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

    // Now, go through the list of code segments and generate the code for
    // them. This MUST be done in reverse order, as the jump commands must
    // know how far forward they must jump, and they can only know that if
    // they know how long all of the following segments are
    for (WhirlCodeSegment *segment = cur_segment; segment != NULL; segment = segment->prev) {
        generate_segment(segment, ring_state);
    }

    output_segments(head_segment);
    free_segments(head_segment);
}
