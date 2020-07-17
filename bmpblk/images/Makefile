# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This makefile lets you MANUALLY create a set of images and the corresponding
# DEFAUL.yaml file for a particular platform. It should NOT be run
# automatically, but only when the reference images have changed following review
# by the localization team. Additionally, it needs to be run outside of the
# chroot, since it uses ImageMagick to transform the reference images, and
# ImageMagick is much to complex to bother installing into the chroot just for
# this occasional need.

TARGETS ?= ALL
OUTPUT ?= ../build

# Default locales for each target is defined in board.yaml.
LOCALES ?=

default:
	LOCALES="$(LOCALES)" ./build_images.py std

all:
	LOCALES="$(LOCALES)" ./build_images.py $(TARGETS)

%:
	LOCALES="$(LOCALES)" ./build_images.py $@

clean:
	rm -rf $(OUTPUT)/*

.PHONY: $(TARGETS) clean all default
