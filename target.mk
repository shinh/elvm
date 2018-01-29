ifeq ($(TOOL),)
ifneq ($(findstring out/,$(RUNNER)),)
else ifneq ($(findstring tools/,$(RUNNER)),)
else
TOOL := $(firstword $(RUNNER))
endif
endif

ifneq ($(CAN_BUILD),)
else ifeq ($(TOOL),)
CAN_BUILD := 1
else ifneq ($(shell which $(TOOL)),)
CAN_BUILD := 1
endif

ifeq ($(CAN_BUILD),1)

include clear_vars.mk
SRCS := $(OUT.eir)
EXT := $(TARGET)
$(eval CMD = $$(ELC) -$(TARGET) $$2 > $$1.tmp && chmod 755 $$1.tmp && mv $$1.tmp $$1)
OUT.eir.$(TARGET) := $(SRCS:%=%.$(EXT))
include build.mk
build-$(TARGET): $(OUT.eir.$(TARGET))

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

test-$(TARGET): $(DIFFS)
$(TARGET): test-$(TARGET)

# Make sure elc.c.eir can create the same target.

include clear_vars.mk
SRCS := $(OUT.eir)
EXT := elc.$(TARGET)
DEPS := $(ELC_EIR)
$(eval CMD = (echo $(TARGET) && cat $$2) | $(ELC_EIR) > $$1.tmp && mv $$1.tmp $$1)
OUT.eir.elc.$(TARGET) := $(SRCS:%=%.$(EXT))
include build.mk

include clear_vars.mk
EXPECT := eir.$(TARGET)
ACTUAL := eir.elc.$(TARGET)
include diff.mk

elc-$(TARGET): $(DIFFS)
test-$(TARGET)-full: elc-$(TARGET)

else

$(info Skip building $(TARGET) due to lack of $(TOOL))

$(TARGET) elc-$(TARGET):
	@echo "*** Skip building $@ ***"

endif  # CAN_BUILD

TEST_FILTER :=
TOOL :=
CAN_BUILD :=
