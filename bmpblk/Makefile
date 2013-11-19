# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This will regenerate the BIOS bitmap images for all platforms. You
# shouldn't need to do this, though.

STAGE ?= build/.stage

default: outside_chroot strings images

outside_chroot:
	@if [ -e /etc/debian_chroot ]; then \
		echo "PIL color quantization is broken inside the chroot."; \
		echo "You must be outside the chroot to do this"; \
		echo "(and you probably shouldn't be doing it anyway)."; \
		exit 1; \
	fi

strings:
	$(MAKE) -C strings

images:
	$(MAKE) -C images all

clean:
	$(MAKE) -C strings clean
	$(MAKE) -C images clean

$(STAGE):
	$(MAKE) -C strings

.DEFAULT:
	$(MAKE) outside_chroot $(STAGE)
	$(MAKE) -C images $@

.PHONY: outside_chroot strings images clean
