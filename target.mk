include clear_vars.mk
SRCS := $(OUT.eir)
EXT := $(TARGET)
CMD = $(ELC) -$(TARGET) $2 > $1.tmp && mv $1.tmp $1
OUT.eir.$(TARGET) := $(SRCS:%=%.$(EXT))
include build.mk

include clear_vars.mk
SRCS := $(OUT.eir.$(TARGET))
EXT := out
DEPS := $(TEST_INS) runtest.sh
CMD = ./runtest.sh $1 $(RUNNER) $2
OUT.eir.$(TARGET).out := $(SRCS:%=%.$(EXT))
include build.mk

include clear_vars.mk
EXPECT := eir.out
ACTUAL := eir.$(TARGET).out
include diff.mk
