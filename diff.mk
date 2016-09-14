DIFFS := $(addprefix out/,$(addsuffix .diff,$(OUT.$(ACTUAL))))

$(DIFFS): %.$(ACTUAL).diff: %.$(EXPECT) %.$(ACTUAL)
	(diff $^ > $@.tmp && mv $@.tmp $@) || (cat $@.tmp ; false)

TEST_RESULTS += $(DIFFS)
