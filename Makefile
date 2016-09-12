CFLAGS := -m32 -W -Wall -W -Werror -MMD -O -g

ELI := out/eli
ELC := out/elc
8CC := out/8cc
BINS := $(8CC) $(ELI) $(ELC) out/dump_ir
LIB_IR := out/ir.o out/table.o

all: test

CSRCS := ir/ir.c ir/table.c ir/dump_ir.c ir/eli.c
COBJS := $(addprefix out/,$(notdir $(CSRCS:.c=.o)))
$(COBJS): out/%.o: ir/%.c
	$(CC) -c $(CFLAGS) $< -o $@

ELC_SRCS := elc.c util.c rb.c py.c js.c x86.c
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

$(8CC): $(wildcard 8cc/*.c 8cc/*.h)
	$(MAKE) -C 8cc && cp 8cc/8cc $@

# Stage tests

$(shell mkdir -p out)

SRCS := $(wildcard test/*.eir)
DSTS := $(SRCS:test/%.eir=out/%.eir)
$(DSTS): out/%.eir: test/%.eir
	cp $< $@.tmp && mv $@.tmp $@
OUT.eir := $(SRCS:test/%.eir=%.eir)

SRCS := $(wildcard test/*.c)
DSTS := $(SRCS:test/%.c=out/%.c)
$(DSTS): out/%.c: test/%.c
	cp $< $@.tmp && mv $@.tmp $@
OUT.c := $(SRCS:test/%.c=%.c)

out/8cc.c:
	./merge_8cc.sh > $@.tmp && mv $@.tmp $@
OUT.c += 8cc.c

# Build tests

TEST_INS := $(wildcard test/*.in)

include clear_vars.mk
SRCS := $(OUT.c)
EXT := exe
CMD = $(CC) -Ilibc $2 -o $1
OUT.c.exe := $(SRCS:%=%.$(EXT))
include build.mk

include clear_vars.mk
SRCS := $(OUT.c.exe)
EXT := out
DEPS := $(TEST_INS) runtest.sh
CMD = ./runtest.sh $1 $2
include build.mk

include clear_vars.mk
SRCS := $(OUT.c)
EXT := eir
CMD = $(8CC) -S -Ilibc $2 -o $1
OUT.eir += $(SRCS:%=%.$(EXT))
include build.mk

include clear_vars.mk
SRCS := $(OUT.eir)
EXT := out
CMD = ./runtest.sh $1 $(ELI) $2
include build.mk

# Ruby backend

include clear_vars.mk
SRCS := $(OUT.eir)
EXT := rb
CMD = $(ELC) -rb $2 > $1.tmp && mv $1.tmp $1
OUT.eir.rb := $(SRCS:%=%.$(EXT))
include build.mk

include clear_vars.mk
SRCS := $(OUT.eir.rb)
EXT := out
CMD = ./runtest.sh $1 ruby $2
include build.mk

# Python backend

include clear_vars.mk
SRCS := $(OUT.eir)
EXT := py
CMD = $(ELC) -py $2 > $1.tmp && mv $1.tmp $1
OUT.eir.py := $(SRCS:%=%.$(EXT))
include build.mk

include clear_vars.mk
SRCS := $(OUT.eir.py)
EXT := out
CMD = ./runtest.sh $1 python $2
include build.mk

# JS backend

include clear_vars.mk
SRCS := $(OUT.eir)
EXT := js
CMD = $(ELC) -js $2 > $1.tmp && mv $1.tmp $1
OUT.eir.js := $(SRCS:%=%.$(EXT))
include build.mk

include clear_vars.mk
SRCS := $(OUT.eir.js)
EXT := out
CMD = ./runtest.sh $1 nodejs $2
include build.mk

# x86 backend

include clear_vars.mk
SRCS := $(OUT.eir)
EXT := x86
CMD = $(ELC) -x86 $2 > $1.tmp && chmod 755 $1.tmp && mv $1.tmp $1
OUT.eir.x86 := $(SRCS:%=%.$(EXT))
include build.mk

include clear_vars.mk
SRCS := $(OUT.eir.x86)
EXT := out
CMD = ./runtest.sh $1 $2
include build.mk

test: $(TEST_RESULTS)

-include */*.d
