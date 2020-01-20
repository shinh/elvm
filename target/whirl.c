#include <ir/ir.h>
#include <target/util.h>
#include <stdlib.h>

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
} RingState;

typedef struct CodeSegment_ {
    Inst *inst;

    char *code;

    size_t alloc, len;

    struct CodeSegment_ *next, *prev;
} WhirlCodeSegment;

#define INIT_CODE_ALLOC 32

WhirlCodeSegment *new_segment(Inst *inst, WhirlCodeSegment *prev) {
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

void output_segments(const WhirlCodeSegment *segment) {
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

void free_segments(WhirlCodeSegment *head) {
    while (head != NULL) {
        WhirlCodeSegment *tmp = head;
        head = head->next;
        free(tmp->code);
        free(tmp);
    }
}

void generate_segment(WhirlCodeSegment *segment, RingState *state) {
    (void) state;

    switch (segment->inst->op) {
        case DUMP:
        default:
            // Do nothing
            break;
    }
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
