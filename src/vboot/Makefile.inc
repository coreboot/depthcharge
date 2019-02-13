##
## Copyright 2012 Google Inc.
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; version 2 of the License.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##

subdirs-y := callbacks crossystem stages util

depthcharge-y += stages.c
fastboot-y += fastboot.c
depthcharge-y += boot_policy.c
depthcharge-y += firmware_id.c
depthcharge-y += screens.c
depthcharge-y += vbnv.c

# Where "main" lives.
vboot-y += main.c

VB_LIB = $(obj)/external/vboot/vboot_fw.a

# The vboot library source is external to depthcharge.
$(VB_LIB):
ifndef VB_SOURCE
	$(error Set VB_SOURCE to point to the vboot source directory)
else
	@printf "    MAKE       $(subst $(obj)/,,$(@))\n"
	$(Q)FIRMWARE_ARCH=$(VB_FIRMWARE_ARCH) \
		CC="$(XCC)" \
		CFLAGS="$(CFLAGS) -DVBOOT_DEBUG" \
		$(MAKE) -C $(VB_SOURCE) \
		MOCK_TPM=$(CONFIG_MOCK_TPM) \
		TPM2_MODE=$(CONFIG_TPM2_MODE) \
		DETACHABLE_UI=$(CONFIG_DETACHABLE_UI) \
		VENDOR_DATA_LENGTH=$(CONFIG_VENDOR_DATA_LENGTH) \
		DIAGNOSTIC_UI=$(CONFIG_DIAGNOSTIC_UI) \
		CUSTOM_MUSIC=$(CONFIG_CUSTOM_MUSIC) \
		BUILD=$(abspath $(dir $(VB_LIB))) \
		V=$(V) \
		fwlib
endif

.PHONY: $(VB_LIB)
