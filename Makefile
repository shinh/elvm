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
OUT.c := $(CTEST_SRCS:test/%.c=%.c)

# Build tests

TEST_INS := $(wildcard test/*.in)

include clear_vars.mk
SRCS := $(OUT.c)
EXT := exe
CMD = $(CC) -Ilibc $2 -o $1
include build.mk
OUT.c.exe := $(OUT.c:%=%.$(EXT))

include clear_vars.mk
SRCS := $(OUT.c.exe)
EXT := out
DEPS := $(TEST_INS) runtest.sh
CMD = ./runtest.sh $1 $2
include build.mk

test: $(TEST_RESULTS)

-include */*.d
