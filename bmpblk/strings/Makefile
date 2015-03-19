# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

.PHONY: clean all shared_strings font locale

LOCALEDIR=locale
OUTPUT?=../build
OUTDIR=$(OUTPUT)/.stage
FONTSIZE=14

all: shared_strings font $(LOCALEDIR)

$(LOCALEDIR):
	(cd $@ && FONTSIZE="$(FONTSIZE)" ./build.py)

shared_strings:
	mkdir -p "$(OUTDIR)"
	./text_to_png_svg --point=$(FONTSIZE) --outdir=$(OUTDIR) *.TXT

font:
	FONTSIZE="$(FONTSIZE)" ./build_font $(OUTDIR)/$@

clean:
	rm -rf $(OUTDIR)
