# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

OUTDIR=../../_stage/locale
FONTSIZE=

LOCALES ?= ar bg bn ca cs da de el en en_GB es es_419 et fa fi fil fr gu hi hr \
	   hu id it iw ja kn ko lt lv ml mr ms nl no pl pt_BR pt_PT ro ru sk \
	   sl sr sv ta te th tr uk vi zh_CN zh_TW

all:
	OUTDIR=$(OUTDIR) FONTSIZE=$(FONTSIZE) ./build_message $(LOCALES)

clean:
	rm -f */*.png
	rm -rf $(OUTDIR)

.PHONY: clean all
