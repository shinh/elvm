COMMONFLAGS := -W -Wall -W -Werror -MMD -MP -O -g -Wno-missing-field-initializers
CFLAGS := -std=gnu99 $(COMMONFLAGS) -Wno-missing-field-initializers
CXXFLAGS := -std=c++11 $(COMMONFLAGS)

uname := $(shell uname)
ifneq (,$(findstring arm,$(shell uname -m)))
ARCH := arm
else
ARCH := x86
endif

ELI := out/eli
ELC := out/elc
8CC := out/8cc
8CC_SRCS := \
	8cc/buffer.c \
	8cc/cpp.c \
	8cc/debug.c \
	8cc/dict.c \
	8cc/encoding.c \
	8cc/error.c \
	8cc/file.c \
	8cc/gen.c \
	8cc/lex.c \
	8cc/main.c \
	8cc/map.c \
	8cc/parse.c \
	8cc/path.c \
	8cc/set.c \
	8cc/vector.c

BINS := $(8CC) $(ELI) $(ELC) out/dump_ir out/befunge out/bfopt
LIB_IR_SRCS := ir/ir.c ir/table.c
LIB_IR := $(LIB_IR_SRCS:ir/%.c=out/%.o)

ELC_EIR := out/elc.c.eir.c.gcc.exe

all: build
	@echo ''
	@echo 'Now you have tools such as out/8cc and out/elc'
	@echo ''
	@echo 'Use following targets to build/run tests for backend xxx:'
	@echo ''
	@echo '- `make build-xxx`: Compile all tests to xxx backend'
	@echo '   (e.g., out/lisp.c.eir.xxx will be built from test/lisp.c)'
	@echo '- `make test-xxx` or `make xxx`: Run tests for xxx backend'
	@echo '   (e.g., out/lisp.c.eir.xxx.out.diff will be output)'
	@echo '- `make test-xxx-full`: Run target/xxx.c on ELVM'
	@echo '   (this checks if the target xxx can self-host)'
	@echo '- `make test`: Build and run all tests'
	@echo '   (WARNING: will take a lot of time/disk)'
	@echo ''

out/git_submodule.stamp: .git/index
	git submodule update --init
	touch $@

$(8CC_SRCS) Whitespace/whitespace.c tinycc/configure: out/git_submodule.stamp

Whitespace/whitespace.out: Whitespace/whitespace.c
	$(MAKE) -C Whitespace 'MAX_SOURCE_SIZE:=16777216' 'MAX_BYTECODE_SIZE:=16777216' 'MAX_N_LABEL:=1048576' 'HEAP_SIZE:=16777224'

out/befunge: tools/befunge.cc
	$(CXX) $(CXXFLAGS) $< -o $@

out/bfopt: tools/bfopt.cc
	$(CXX) $(CXXFLAGS) $< -o $@

out/tm: tools/tm.cc
	$(CXX) $(CXXFLAGS) $< -o $@

tinycc/tcc: tinycc/config.h
	$(MAKE) -C tinycc tcc libtcc1.a

tinycc/config.h: tinycc/configure
	cd tinycc && ./configure

out/elc.c.eir.c.gcc.exe: out/elc.c.eir.c
	$(CC) -o $@ $<

CSRCS := $(LIB_IR_SRCS) ir/dump_ir.c ir/eli.c
COBJS := $(addprefix out/,$(notdir $(CSRCS:.c=.o)))
$(COBJS): out/%.o: ir/%.c
	$(CC) -c -I. $(CFLAGS) $< -o $@

ELC_SRCS := \
	elc.c \
	util.c \
	asmjs.c \
	arm.c \
	bef.c \
	bf.c \
	c.c \
	cl.c \
	cpp.c \
	cpp_template.c \
	cr.c \
	cs.c \
	el.c \
	forth.c \
	fs.c \
	go.c \
	hs.c \
	i.c \
	java.c \
	js.c \
	lua.c \
	ll.c \
	oct.c \
	php.c \
	piet.c \
	pietasm.c \
	pl.c \
	py.c \
	ps.c \
	rb.c \
	rs.c \
	sed.c \
	sh.c \
	sqlite3.c \
	scala.c \
	scm_sr.c \
	swift.c \
	tex.c \
	tf.c \
	tm.c \
	unl.c \
	vim.c \
	wasm.c \
	ws.c \
	x86.c \

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

SRCS := $(sort $(wildcard test/*.eir))
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
	cp 8cc/*.h 8cc/*.inc 8cc/include/*.h out
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
CMD = $2 -S -o $1.S.tmp - < test/8cc.in.c && sed 's/ *$(sharp).*//' $1.S.tmp > $1.S && (echo === test/8cc.in === && cat $1.S && echo) > $1.tmp && mv $1.tmp $1
include build.mk

include clear_vars.mk
SRCS := $(OUT.c)
EXT := eir
CMD = $(8CC) -S -I. -Ilibc -Iout -o $1.tmp $2 && mv $1.tmp $1
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

ifdef TF
TARGET := tf
RUNNER := python
include target.mk
endif

TARGET := js
RUNNER := nodejs
include target.mk

TARGET := asmjs
RUNNER := nodejs
include target.mk

TARGET := wasm
RUNNER := tools/runwasm.sh
TOOL := wat2wasm
include target.mk

TARGET := php
RUNNER := php
include target.mk

TARGET := el
RUNNER := emacs --no-site-file --script
include target.mk

TARGET := vim
RUNNER := tools/runvim.sh
TOOL := vim
include target.mk

TARGET := tex
RUNNER := tools/runtex.sh
TOOL := tex
include target.mk

TARGET := cl
RUNNER := sbcl --script
include target.mk

TARGET := sh
RUNNER := bash
ifndef FULL
TEST_FILTER := out/fizzbuzz_fast.c.eir.sh out/8cc.c.eir.sh out/eli.c.eir.sh out/dump_ir.c.eir.sh
endif
include target.mk

TARGET := sed
RUNNER := sed -n -f
# Sed backend is so slow.
ifndef FULL
TEST_FILTER := out/24_cmp.c.eir.sed out/24_cmp2.c.eir.sed out/24_muldiv.c.eir.sed out/bitops.c.eir.sed out/copy_struct.c.eir.sed out/eof.c.eir.sed out/fizzbuzz.c.eir.sed out/fizzbuzz_fast.c.eir.sed out/global_struct_ref.c.eir.sed out/lisp.c.eir.sed out/printf.c.eir.sed out/qsort.c.eir.sed out/8cc.c.eir.sed out/elc.c.eir.sed out/dump_ir.c.eir.sed out/eli.c.eir.sed
endif
# Only GNU sed can output NUL.
ifeq ($(findstring GNU,$(shell sed --version)),)
TEST_FILTER += out/04getc.eir.sed
endif
include target.mk

TARGET := java
RUNNER := tools/runjava.sh
TOOL := javac
include target.mk
$(OUT.eir.java.out): tools/runjava.sh

TARGET := scala
RUNNER := tools/runscala.sh
TOOL := scalac
include target.mk
$(OUT.eir.scala.out): tools/runscala.sh

TARGET := swift
RUNNER := tools/runswift.sh
TOOL := swiftc
include target.mk
$(OUT.eir.swift.out): tools/runswift.sh

TARGET := cr
RUNNER := tools/runcr.sh
TOOL := crystal
include target.mk
$(OUT.eir.crystal.out): tools/runcr.sh

TARGET := cs
RUNNER := tools/runcs.sh
CAN_BUILD := $(if $(or $(shell which dotnet),$(and $(shell which mono),$(shell which gmcs))),1,0)
include target.mk
$(OUT.eir.cs.out): tools/runcs.sh

TARGET := c
RUNNER := tools/runc.sh
include target.mk
$(OUT.eir.c.out): tools/runc.sh tinycc/tcc

TARGET := cpp
RUNNER := tools/runcpp.sh
TOOL := g++
include target.mk
$(OUT.eir.cpp.out): tools/runcpp.sh

ifdef CPP_TEMPLATE
TARGET := cpp_template
RUNNER := tools/runcpp_template.sh
TOOL := g++
include target.mk
$(OUT.eir.cpp_template.out): tools/runcpp_template.sh
endif

ifeq ($(uname),Linux)
TARGET := $(ARCH)
RUNNER :=
include target.mk
endif

TARGET := i
RUNNER := tools/runi.sh
TOOL := ick
# INTERCAL backend is 16bit.
TEST_FILTER := $(addsuffix .i,$(filter out/24_%.eir,$(OUT.eir))) out/eof.c.eir.i out/neg.c.eir.i
include target.mk
$(OUT.eir.i.out): tools/runi.sh tinycc/tcc

TARGET := ws
RUNNER := tools/runws.sh
TEST_FILTER := out/eli.c.eir.ws
include target.mk
$(OUT.eir.ws.out): tools/runws.sh Whitespace/whitespace.out tinycc/tcc

TARGET := bef
RUNNER := out/befunge
include target.mk

TARGET := bf
RUNNER := tools/runbf.sh
ifndef FULL
TEST_FILTER := out/eli.c.eir.bf out/dump_ir.c.eir.bf
endif
include target.mk
$(OUT.eir.bf.out): tools/runbf.sh tinycc/tcc

ifdef PIETASM
TARGET := pietasm
RUNNER := tools/runpietasm.sh
include target.mk
$(OUT.eir.piet.out): tools/runpietasm.sh
endif

ifdef PIET
TARGET := piet
RUNNER := tools/runpiet.sh
# Piet backend is 16bit.
TEST_FILTER := $(addsuffix .piet,$(filter out/24_%.c.eir,$(OUT.eir))) out/eof.c.eir.piet out/neg.c.eir.piet
include target.mk
$(OUT.eir.piet.out): tools/runpiet.sh
endif

TARGET := unl
RUNNER := tools/rununl.sh
ifndef FULL
TEST_FILTER := out/eli.c.eir.unl out/dump_ir.c.eir.unl
endif
include target.mk
$(OUT.eir.unl.out): tools/rununl.sh

TARGET := tm
RUNNER := out/tm
TEST_FILTER := out/24_cmp.c.eir.tm out/24_cmp2.c.eir.tm out/24_muldiv.c.eir.tm out/bitops.c.eir.tm out/copy_struct.c.eir.tm out/eof.c.eir.tm out/fizzbuzz.c.eir.tm out/fizzbuzz_fast.c.eir.tm out/global_struct_ref.c.eir.tm out/lisp.c.eir.tm out/printf.c.eir.tm out/qsort.c.eir.tm out/8cc.c.eir.tm out/elc.c.eir.tm out/dump_ir.c.eir.tm out/eli.c.eir.tm
include target.mk
$(OUT.eir.tm.out): out/tm

TARGET := forth
RUNNER := gforth --dictionary-size 16M
include target.mk

TARGET := fs
RUNNER := tools/runfs.sh
CAN_BUILD := $(if $(or $(shell which dotnet),$(and $(shell which mono),$(shell which fsharpc))),1,0)
include target.mk
$(OUT.eir.fs.out): tools/runfs.sh

TARGET := pl
RUNNER := perl
include target.mk

TARGET := go
RUNNER := go run
include target.mk

TARGET := sqlite3
RUNNER := tools/runsqlite3.sh
TOOL := sqlite3
include target.mk

TARGET := ps
RUNNER := gsnd -q -dBATCH --
include target.mk

TARGET := lua
RUNNER := lua
CAN_BUILD := $(shell lua -v 2>&1 | perl -ne 'print /^Lua (\d+)\.(\d+)/ && ($$1 >= 5 && $$2 >= 3) ? 1 : 0')
include target.mk

TARGET := ll
RUNNER := lli
include target.mk

TARGET := scm_sr
RUNNER := tools/runscm_sr.sh
TOOL := gosh
include target.mk

TARGET := hs
RUNNER := tools/runhs.sh
TOOL := ghc
include target.mk
$(OUT.eir.hs.out): tools/runhs.sh

TARGET := oct
RUNNER := octave -q
include target.mk

TARGET := rs
RUNNER := tools/runrs.sh
TOOL := rustc
include target.mk

test: $(TEST_RESULTS)

.SUFFIXES:

-include */*.d
