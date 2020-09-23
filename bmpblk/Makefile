# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This generates BIOS bitmap images for specified board.

OUTPUT ?= build
STAGE ?= $(OUTPUT)/.stage
FONTSIZE = 14
PHYSICAL_PRESENCE ?= keyboard

build:
	@[ ! -z "$(BOARD)" ] || (echo "Usage: BOARD=\$$BOARD make"; exit 1)
	mkdir -p "$(STAGE)"
	./text_to_png_svg --point="$(FONTSIZE)" --outdir="$(STAGE)" strings/*.TXT
	FONTSIZE="$(FONTSIZE)" ./build_font "$(STAGE)/font"
	LOCALES="$(LOCALES)" FONTSIZE="$(FONTSIZE)" ./build.py
	LOCALES="$(LOCALES)" OUTPUT="$(OUTPUT)" \
		PHYSICAL_PRESENCE="$(PHYSICAL_PRESENCE)" \
		./build_images.py "$(BOARD)"

archive:
	./archive_images.py -a "$(ARCHIVER)" -d "$(OUTPUT)"
	"$(ARCHIVER)" "$(OUTPUT)/font.bin" create "$(OUTPUT)"/font/*.bmp

clean:
	rm -rf $(OUTPUT)
	find . -type f -name '*.pyc' -delete

.PHONY: build help archive clean
