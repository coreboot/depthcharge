# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

.PHONY: clean all shared_strings font localized_text

LOCALEDIR=localized_text
OUTDIR=../build/.stage
FONTSIZE=18

all: shared_strings font $(LOCALEDIR)

$(LOCALEDIR):
	make -C $@ OUTDIR="../$(OUTDIR)/locale" FONTSIZE="$(FONTSIZE)"

shared_strings:
	mkdir -p "$(OUTDIR)"
	./text_to_png --point=$(FONTSIZE) --outdir=$(OUTDIR) *.TXT

font:
	FONTSIZE="$(FONTSIZE)" ./build_font $(OUTDIR)/$@

clean:
	rm -rf $(OUTDIR)
