COMMONFLAGS := -W -Wall -W -Werror -MMD -MP -O -g -Wno-missing-field-initializers
CFLAGS := -std=gnu99 -m32 $(COMMONFLAGS) -Wno-missing-field-initializers
CXXFLAGS := $(COMMONFLAGS)

ELI := out/eli
ELC := out/elc
8CC := out/8cc
8CC_SRCS := \
	8cc/main.c \
	8cc/cpp.c \
	8cc/error.c \
	8cc/lex.c \
	8cc/parse.c \
	8cc/debug.c \
	8cc/list.c \
	8cc/string.c \
	8cc/dict.c \
	8cc/gen.c

BINS := $(8CC) $(ELI) $(ELC) out/dump_ir out/bfopt tinycc/tcc
LIB_IR_SRCS := ir/ir.c ir/table.c
LIB_IR := $(LIB_IR_SRCS:ir/%.c=out/%.o)

#ELC_EIR := out/eli out/elc.c.eir
ELC_EIR := out/elc.c.eir.x86

all: test

out/git_submodule.stamp: .git/index
	git submodule update --init
	touch $@

$(8CC_SRCS) Whitespace/whitespace.c tinycc/configure: out/git_submodule.stamp

Whitespace/whitespace.out: Whitespace/whitespace.c
	$(MAKE) -C Whitespace 'MAX_SOURCE_SIZE:=16777216' 'MAX_BYTECODE_SIZE:=16777216' 'MAX_N_LABEL:=1048576' 'HEAP_SIZE:=16777224'

out/bfopt: tools/bfopt.cc
	$(CXX) $(CXXFLAGS) $< -o $@

tinycc/tcc: tinycc/config.h
	$(MAKE) -C tinycc tcc libtcc1.a

tinycc/config.h: tinycc/configure
	cd tinycc && ./configure

CSRCS := $(LIB_IR_SRCS) ir/dump_ir.c ir/eli.c
COBJS := $(addprefix out/,$(notdir $(CSRCS:.c=.o)))
$(COBJS): out/%.o: ir/%.c
	$(CC) -c -I. $(CFLAGS) $< -o $@

ELC_SRCS := elc.c util.c rb.c py.c js.c x86.c ws.c bf.c
ELC_SRCS := $(addprefix target/,$(ELC_SRCS))
COBJS := $(addprefix out/,$(notdir $(ELC_SRCS:.c=.o)))
$(COBJS): out/%.o: target/%.c
	$(CC) -c -I. $(CFLAGS) $< -o $@

out/dump_ir: $(LIB_IR) out/dump_ir.o
	$(CC) $(CFLAGS) -DTEST $^ -o $@

$(ELI): $(LIB_IR) out/eli.o
	$(CC) $(CFLAGS) $^ -o $@

$(ELC): $(LIB_IR) $(ELC_SRCS:target/%.c=out/%.o)
	$(CC) $(CFLAGS) $^ -o $@

$(8CC): $(8CC_SRCS)
	$(MAKE) -C 8cc && cp 8cc/8cc $@

# Stage tests

$(shell mkdir -p out)
TEST_RESULTS :=

SRCS := $(wildcard test/*.eir)
DSTS := $(SRCS:test/%.eir=out/%.eir)
OUT.eir := $(DSTS)
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

out/8cc.c: $(8CC_SRCS)
	cp 8cc/*.h out
	cat $(8CC_SRCS) > $@.tmp && mv $@.tmp $@
OUT.c += out/8cc.c

out/elc.c: $(ELC_SRCS) $(LIB_IR_SRCS)
	cat $^ > $@.tmp && mv $@.tmp $@
OUT.c += out/elc.c

out/dump_ir.c: ir/dump_ir.c $(LIB_IR_SRCS)
	cat $^ > $@.tmp && mv $@.tmp $@
OUT.c += out/dump_ir.c

out/eli.c: ir/eli.c $(LIB_IR_SRCS)
	cat $^ > $@.tmp && mv $@.tmp $@
OUT.c += out/eli.c

# Build tests

TEST_INS := $(wildcard test/*.in)

include clear_vars.mk
SRCS := $(OUT.c)
EXT := exe
CMD = $(CC) -std=gnu99 -DNOFILE -include libc/_builtin.h -I. $2 -o $1
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
CMD = $(8CC) -S -DNOFILE -I. -Ilibc $2 -o $1.tmp && mv $1.tmp $1
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

build: $(TEST_RESULTS)

# Targets

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

TARGET := bf
RUNNER := tools/runbf.sh
ifdef FULL
TEST_FILTER := out/eli.c.eir.bf out/dump_ir.c.eir.bf
endif
include target.mk
$(OUT.eir.bf.out): tools/runbf.sh

test: $(TEST_RESULTS)

.SUFFIXES:

-include */*.d
