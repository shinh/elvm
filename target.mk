include clear_vars.mk
SRCS := $(OUT.eir)
EXT := $(TARGET)
$(eval CMD = $$(ELC) -$(TARGET) $$2 > $$1.tmp && chmod 755 $$1.tmp && mv $$1.tmp $$1)
OUT.eir.$(TARGET) := $(SRCS:%=%.$(EXT))
include build.mk

include clear_vars.mk

SRCS := $(filter-out $(TEST_FILTER),$(OUT.eir.$(TARGET)))
EXT := out
DEPS := $(TEST_INS) runtest.sh
$(eval CMD = ./runtest.sh $$1 $(RUNNER) $$2)
OUT.eir.$(TARGET).out := $(SRCS:%=%.$(EXT))
include build.mk

include clear_vars.mk
EXPECT := eir.out
ACTUAL := eir.$(TARGET).out
include diff.mk

$(TARGET): $(DIFFS)

# Make sure elc.c.eir can create the same target.

ifneq ($(TARGET),x86)
ifneq ($(TARGET),ws)

include clear_vars.mk
SRCS := $(OUT.eir)
EXT := elc.$(TARGET)
DEPS := $(ELC_EIR)
$(eval CMD = (echo $(TARGET) && cat $$2) | $(ELC_EIR) > $$1.tmp && mv $$1.tmp $$1)
OUT.eir.elc.$(TARGET) := $(SRCS:%=%.$(EXT))
include build.mk

#include clear_vars.mk
#SRCS := $(OUT.eir:%=%.elc.$(TARGET))
#EXT := out
#DEPS := $(TEST_INS) runtest.sh
#$(eval CMD = ./runtest.sh $$1 $(RUNNER) $$2)
#include build.mk

include clear_vars.mk
EXPECT := eir.$(TARGET)
ACTUAL := eir.elc.$(TARGET)
include diff.mk

endif
endif

TEST_FILTER :=
