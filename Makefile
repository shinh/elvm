CFLAGS := -std=gnu99 -m32 -W -Wall -W -Werror -MMD -O -g -Wno-missing-field-initializers

ELI := out/eli
ELC := out/elc
8CC := out/8cc
8CC_SRCS := $(wildcard 8cc/*.c 8cc/*.h)
BINS := $(8CC) $(ELI) $(ELC) out/dump_ir
LIB_IR := out/ir.o out/table.o

all: test

8cc/main.c Whitespace/whitespace.c:
	git submodule update --init

Whitespace/whitespace.out:
	$(MAKE) -C Whitespace 'MAX_SOURCE_SIZE:=16777216' 'MAX_BYTECODE_SIZE:=16777216' 'MAX_N_LABEL:=1048576' 'HEAP_SIZE:=16777224'

CSRCS := ir/ir.c ir/table.c ir/dump_ir.c ir/eli.c
COBJS := $(addprefix out/,$(notdir $(CSRCS:.c=.o)))
$(COBJS): out/%.o: ir/%.c
	$(CC) -c $(CFLAGS) $< -o $@

ELC_SRCS := elc.c util.c rb.c py.c js.c x86.c ws.c
CSRCS := $(addprefix ir/,$(ELC_SRCS))
COBJS := $(addprefix out/,$(notdir $(CSRCS:.c=.o)))
$(COBJS): out/%.o: target/%.c
	$(CC) -c -I. $(CFLAGS) $< -o $@

out/dump_ir: $(LIB_IR) out/dump_ir.o
	$(CC) $(CFLAGS) -DTEST $^ -o $@

$(ELI): $(LIB_IR) out/eli.o
	$(CC) $(CFLAGS) $^ -o $@

$(ELC): $(LIB_IR) $(ELC_SRCS:%.c=out/%.o)
	$(CC) $(CFLAGS) $^ -o $@

$(8CC): $(8CC_SRCS)
	$(MAKE) -C 8cc && cp 8cc/8cc $@

# Stage tests

$(shell mkdir -p out)
TEST_RESULTS :=

SRCS := $(wildcard test/*.eir)
OUT.eir := $(DSTS)
DSTS := $(SRCS:test/%.eir=out/%.eir)
$(DSTS): out/%.eir: test/%.eir
	cp $< $@.tmp && mv $@.tmp $@

SRCS := $(wildcard test/*.eir.rb)
DSTS := $(SRCS:test/%.eir.rb=out/%.eir)
OUT.eir += $(DSTS)
$(DSTS): out/%.eir: test/%.eir.rb
	ruby $< > $@.tmp && mv $@.tmp $@

SRCS := $(wildcard test/*.c)
DSTS := $(SRCS:test/%.c=out/%.c)
$(DSTS): out/%.c: test/%.c
	cp $< $@.tmp && mv $@.tmp $@
OUT.c := $(SRCS:test/%.c=out/%.c)

out/8cc.c: merge_8cc.sh libc/libf.h $(8CC_SRCS)
	./$< > $@.tmp && mv $@.tmp $@
OUT.c += out/8cc.c

# Build tests

TEST_INS := $(wildcard test/*.in)

include clear_vars.mk
SRCS := $(OUT.c)
EXT := exe
CMD = $(CC) -std=gnu99 $2 -o $1
OUT.c.exe := $(SRCS:%=%.$(EXT))
include build.mk

include clear_vars.mk
SRCS := $(filter-out out/8cc.c.exe,$(OUT.c.exe))
EXT := out
DEPS := $(TEST_INS) runtest.sh
CMD = ./runtest.sh $1 $2
include build.mk

include clear_vars.mk
SRCS := out/8cc.c.exe
EXT := out
DEPS := $(TEST_INS)
# TODO: Hacky!
sharp := \#
CMD = $2 -S -o $1.S test/8cc.in.c && sed -i 's/ *$(sharp).*//' $1.S && (echo === test/8cc.in === && cat $1.S && echo) > $1.tmp && mv $1.tmp $1
include build.mk

include clear_vars.mk
SRCS := $(OUT.c)
EXT := eir
CMD = $(8CC) -S -Ilibc $2 -o $1
DEPS := $(wildcard libc/*.h)
OUT.eir += $(SRCS:%=%.$(EXT))
include build.mk

include clear_vars.mk
SRCS := $(OUT.eir)
EXT := out
DEPS := $(TEST_INS) runtest.sh
CMD = ./runtest.sh $1 $(ELI) $2
OUT.eir.out := $(SRCS:%=%.$(EXT))
include build.mk

include clear_vars.mk
OUT.c.exe.out := $(OUT.c.exe:%=%.out)
OUT.c.eir.out := $(OUT.c.exe.out:%.c.exe.out=%.c.eir.out)
EXPECT := c.exe.out
ACTUAL := c.eir.out
include diff.mk

TARGET := rb
RUNNER := ruby
include target.mk

TARGET := py
RUNNER := python
include target.mk

TARGET := js
RUNNER := nodejs
include target.mk

TARGET := x86
RUNNER :=
include target.mk

TARGET := ws
RUNNER := tools/runws.sh
include target.mk
$(OUT.eir.ws.out): tools/runws.sh Whitespace/whitespace.out

test: $(TEST_RESULTS)

.SUFFIXES:

-include */*.d
