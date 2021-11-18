# SPDX-License-Identifier: GPL-2.0

TEST_PRINT := 1

include $(src)/tests/Makefile.inc

screenshotsrc := $(src)/screenshot
screenshotobj = $(obj)/screenshot
optobj = $(screenshotobj)/options

# Screenshot options
WIDTH ?= 1920
HEIGHT ?= 1080
SCREEN ?= UI_SCREEN_RECOVERY_SELECT
ITEM ?= 0
DISABLED_ITEM_MASK ?= 0
HIDDEN_ITEM_MASK ?= 0
PAGE ?= 0
LOG ?=
override LOG := \"$(LOG)\"
LOG_FILE ?=
override LOG_FILE := \"$(LOG_FILE)\"
BMP_PATH ?= $(screenshotsrc)/bmp
override BMP_PATH := \"$(BMP_PATH)\"
OUTPUT ?= $(obj)/output.bmp

SCREENSHOT_OPTIONS += WIDTH HEIGHT SCREEN ITEM
SCREENSHOT_OPTIONS += DISABLED_ITEM_MASK HIDDEN_ITEM_MASK
SCREENSHOT_OPTIONS += PAGE LOG LOG_FILE BMP_PATH

$(foreach opt, $(SCREENSHOT_OPTIONS), \
	$(eval UI_FLAGS += -D__$(opt)__=$($(opt))))

mainsrc := screenshot/main.c
archivesrc := screenshot/archive.c

screenshot-srcs += $(mainsrc)
screenshot-srcs += $(archivesrc)
screenshot-srcs += screenshot/mocks/vb2api.c
screenshot-srcs += screenshot/stubs/display.c
screenshot-srcs += screenshot/stubs/ui.c
screenshot-srcs += src/vboot/ui/bitmap.c
screenshot-srcs += src/vboot/ui/display.c
screenshot-srcs += src/vboot/ui/draw.c
screenshot-srcs += src/vboot/ui/layout.c
screenshot-srcs += src/vboot/ui/log.c
screenshot-srcs += src/vboot/ui/screens.c
screenshot-srcs += tests/stubs/boot/payload.c
screenshot-srcs += tests/stubs/diag.c

screenshot-srcobjs := $(addprefix $(screenshotobj)/, \
	$(patsubst %.c,%.o,$(filter src/%,$(screenshot-srcs))))

screenshot-objs := $(addprefix $(screenshotobj)/, \
	$(patsubst %.c,%.o,$(screenshot-srcs)))
screenshot-objs += $(addprefix $(screenshotobj)/, \
	$(patsubst %.c,%.o,tests/stubs/heap.c $(default_mocks-srcs)))
$(screenshot-objs): TEST_CFLAGS += $(UI_FLAGS)

# Always rebuild force-srcs as screenshot options (such as SCREEN) might change.
force-srcs := $(mainsrc) $(archivesrc)
force-objs := $(addprefix $(screenshotobj)/,$(patsubst %.c,%.o,$(force-srcs)))
.FORCE:
$(force-objs): .FORCE

screenshot-mocks += vb2api_diagnostic_ui_enabled
screenshot-mocks += vb2api_gbb_read_hwid
screenshot-mocks += vb2api_phone_recovery_enabled
screenshot-mocks += vb2api_phone_recovery_ui_enabled

# Files that should not include libpayload headers (such as stdio.h)
syslibc_srcs := screenshot/io.c
syslibc_objs := $(addprefix $(screenshotobj)/, \
	$(patsubst %.c,%.o,$(syslibc_srcs)))

$(syslibc_objs): $(obj)/screenshot/%.o: %.c
	@printf "    CC       $(subst $(obj)/,,$@)\n"
	$(HOSTCC) $(HOSTCFLAGS) -MMD -MF $(basename $@).d -MT $@ -c $< -o $@

screenshot-bin := $(screenshotobj)/run
$(screenshot-bin): $(syslibc_objs)

$(eval $(call TEST_CC_template,screenshot))

DEPENDENCIES += $(addsuffix .d,$(basename $(screenshot-objs) $(syslibc_objs)))
-include $(DEPENDENCIES)

OUTPUT_RAW := $(screenshotobj)/$(notdir $(basename $(OUTPUT))).raw

.PHONY: screenshot clean-screenshot

screenshot: $(screenshot-bin)
	$(screenshot-bin) $(OUTPUT_RAW)
	$(screenshotsrc)/raw2bmp.py $(WIDTH) $(HEIGHT) $(OUTPUT_RAW) $(OUTPUT)
	@echo Screenshot saved to $(OUTPUT)

clean-screenshot:
	rm -rf $(screenshotobj)
