# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This makefile lets you MANUALLY create a set of images and the corresponding
# DEFAUL.yaml file for a particular platform. It should NOT be run
# automatically, but only when the master images have changed following review
# by the localization team. Additionally, it needs to be run outside of the
# chroot, since it uses ImageMagick to transform the master images, and
# ImageMagick is much to complex to bother installing into the chroot just for
# this occasional need.

TARGETS ?= butterfly falco link parrot peach_pit snow spring std stout

# These are all the known locales, sorted more-or-less geograpically
ALL_LOCALES=en es_419 pt_BR en_GB fr es pt_PT ca it de \
  el nl da no sv fi et lv lt ru pl cs sk hu sl sr hr bg ro \
  uk tr iw ar fa hi th vi id fil zh_CN zh_TW ko ja

ifeq ($(LOCALES),)
# Here are the launch locales for Stumpy/Lumpy (issue 6595), same ordering.
# TODO(hungte) Make LOCALES also configurable per profile (board).
LOCALES=en es_419 pt_BR fr es it de nl da no sv ko ja

# Added more locales for future (crosbug.com/p/12846).
LOCALES+= id th

# More locales (crosbug.com/p/11969)
LOCALES+= ar ms zh_CN zh_TW

# More locales (crosbug.com/p/17592)
LOCALES+= fi pl
endif

default: std

all: $(TARGETS)

$(TARGETS):
	LOCALES="$(LOCALES)" ./build_images "$@"

clean:
	rm -rf ../build/*

.PHONY: $(TARGETS) clean all default
