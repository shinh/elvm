CFLAGS := -m32 -W -Wall -MMD -O -g

BINS := ir_test eli

all: test

ir_test: ir/table.c ir/ir.c
	$(CC) $(CFLAGS) -DTEST $^ -o $@

eli: ir/table.c ir/ir.c ir/eli.c
	$(CC) $(CFLAGS) $^ -o $@

# Stage tests

$(shell mkdir -p out)
CTEST_SRCS := $(wildcard test/*.c)
CTEST_STAGED := $(CTEST_SRCS:test/%.c=out/%.c)
$(CTEST_STAGED): out/%.c: test/%.c
	cp $< $@.tmp && mv $@.tmp $@
CTESTS := $(CTEST_SRCS:test/%.c=%.c)

# Build tests

include clear_vars.mk
SRCS := $(CTESTS)
EXT := $(CC)
CMD = $(CC) -Ilibc $2 -o $1
include build.mk

test: $(TEST_RESULTS)

-include */*.d
