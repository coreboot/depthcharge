# SPDX-License-Identifier: GPL-2.0

# Setup tests/Makefile.common
TEST_PRINT := 1
testobj := $(abspath $(obj)/screenshot)
objutil := $(testobj)/util
objext := $(testobj)/external

include $(src)/tests/Makefile.common

screenshotsrc := $(src)/screenshot
screenshotobj = $(testobj)/screenshot

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
OUTPUT ?= $(screenshotobj)/output.bmp

SCREENSHOT_OPTIONS += WIDTH HEIGHT SCREEN ITEM
SCREENSHOT_OPTIONS += DISABLED_ITEM_MASK HIDDEN_ITEM_MASK
SCREENSHOT_OPTIONS += PAGE LOG LOG_FILE BMP_PATH

$(foreach opt, $(SCREENSHOT_OPTIONS), \
	$(eval UI_FLAGS += -D__$(opt)__=$($(opt))))

mainsrc := screenshot/main.c
archivesrc := screenshot/archive.c

screenshot-srcs += $(mainsrc)
screenshot-srcs += $(archivesrc)
screenshot-srcs += screenshot/mocks/boot/payload.c
screenshot-srcs += screenshot/mocks/cbmem_console.c
screenshot-srcs += screenshot/mocks/vb2api.c
screenshot-srcs += screenshot/stubs/beep.c
screenshot-srcs += screenshot/stubs/diag.c
screenshot-srcs += screenshot/stubs/display.c
screenshot-srcs += screenshot/stubs/firmware_id.c
screenshot-srcs += screenshot/stubs/input.c
screenshot-srcs += src/base/list.c
screenshot-srcs += src/vboot/ui/bitmap.c
screenshot-srcs += src/vboot/ui/display.c
screenshot-srcs += src/vboot/ui/draw.c
screenshot-srcs += src/vboot/ui/layout.c
screenshot-srcs += src/vboot/ui/log.c
screenshot-srcs += src/vboot/ui/loop.c
screenshot-srcs += src/vboot/ui/menu.c
screenshot-srcs += src/vboot/ui/navigation.c
screenshot-srcs += src/vboot/ui/screens.c

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

screenshot-mocks += cbmem_console_snapshot
screenshot-mocks += vb2api_diagnostic_ui_enabled
screenshot-mocks += vb2api_gbb_read_hwid
screenshot-mocks += vb2api_phone_recovery_enabled
screenshot-mocks += vb2api_phone_recovery_ui_enabled

# Files that should not include libpayload headers (such as stdio.h)
syslibc_srcs := screenshot/io.c
syslibc_objs := $(addprefix $(screenshotobj)/, \
	$(patsubst %.c,%.o,$(syslibc_srcs)))

$(syslibc_objs): $(screenshotobj)/%.o: %.c
	@printf "    CC       $(subst $(screenshotobj)/,,$@)\n"
	mkdir -p $(dir $@)
	$(HOSTCC) $(HOSTCFLAGS) -MMD -MF $(basename $@).d -MT $@ -c $< -o $@

screenshot-bin := $(screenshotobj)/run
$(screenshot-bin): $(syslibc_objs)

$(eval $(call TEST_CC_template,screenshot))

DEPENDENCIES += $(addsuffix .d,$(basename $(screenshot-objs) $(syslibc_objs)))
-include $(DEPENDENCIES)

OUTPUT_RAW := $(screenshotobj)/$(notdir $(basename $(OUTPUT))).raw

.PHONY: screenshot test-screenshot clean-screenshot

screenshot: $(screenshot-bin)
	$(screenshot-bin) $(OUTPUT_RAW)
	$(screenshotsrc)/raw2bmp.py $(WIDTH) $(HEIGHT) $(OUTPUT_RAW) $(OUTPUT)
	@echo Screenshot saved to $(OUTPUT)

test-screenshot: screenshot
	$(MAKE) screenshot
	$(MAKE) screenshot SCREEN=UI_SCREEN_FIRMWARE_SYNC
	$(MAKE) screenshot SCREEN=UI_SCREEN_DEBUG_INFO
	$(MAKE) screenshot SCREEN=UI_SCREEN_DEVELOPER_MODE
	$(MAKE) screenshot SCREEN=UI_SCREEN_DEVELOPER_SELECT_ALTFW ITEM=1

clean-screenshot:
	rm -rf $(testobj)
