DSTS := $(SRCS:%=%.$(EXT))

$(eval $(DSTS): PRIV_CMD = $(value CMD))
$(DSTS): %.$(EXT): % $(BINS) $(DEPS)
	$(call PRIV_CMD,$@,$<)

TEST_RESULTS += $(DSTS)
