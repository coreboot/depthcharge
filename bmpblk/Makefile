# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This will regenerate the BIOS bitmap images for all platforms. You
# shouldn't need to do this, though.

OUTPUT ?= build
STAGE ?= $(OUTPUT)/.stage

default: strings images

strings:
	$(MAKE) -C strings

images:
	$(MAKE) -C images all

archive:
	./archive_images.py -a $(ARCHIVER) -d $(OUTPUT)
	${ARCHIVER} "${OUTPUT}/font.bin" create "${OUTPUT}"/font/*.bmp

clean:
	$(MAKE) -C strings clean
	$(MAKE) -C images clean
	rm -rf $(OUTPUT)
	find . -type f -name '*.pyc' -delete

$(STAGE):
	$(MAKE) -C strings

.DEFAULT:
	$(MAKE) $(STAGE)
	$(MAKE) -C images $@

.PHONY: strings images clean
