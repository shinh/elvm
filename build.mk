SRCS := $(SRCS:%=out/%)
DSTS := $(SRCS:%=%.$(EXT))

$(eval $(DSTS): PRIV_CMD = $(value CMD))
$(DSTS): %.$(EXT): % $(BINS)
	$(call PRIV_CMD,$@,$<)

TEST_RESULTS += $(DSTS)
