DIFFS := $(addsuffix .diff,$(OUT.$(ACTUAL)))

$(DIFFS): %.$(ACTUAL).diff: %.$(EXPECT) %.$(ACTUAL)
	(diff -u $^ > $@.tmp && mv $@.tmp $@) || (cat $@.tmp ; false)

TEST_RESULTS += $(DIFFS)
