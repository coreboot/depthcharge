# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This generates BIOS bitmap images for specified board.

OUTPUT ?= build
STAGE ?= $(OUTPUT)/.stage
PHYSICAL_PRESENCE ?= keyboard
ARCHIVER ?= /usr/bin/archive

build:
	@[ ! -z "$(BOARD)" ] || (echo "Usage: BOARD=\$$BOARD make"; exit 1)
	LOCALES="$(LOCALES)" \
		OUTPUT="$(OUTPUT)" \
		PHYSICAL_PRESENCE="$(PHYSICAL_PRESENCE)" \
		./build.py "$(BOARD)"

archive:
	./archive_images.py -a "$(ARCHIVER)" -d "$(OUTPUT)"
	"$(ARCHIVER)" "$(OUTPUT)/font.bin" create "$(OUTPUT)"/glyph/*.bmp

clean:
	rm -rf $(OUTPUT)
	find . -type f -name '*.pyc' -delete

.PHONY: build help archive clean
