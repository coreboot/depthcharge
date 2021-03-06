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

subdirs-y := callbacks crossystem nvdata stages ui util

depthcharge-y += stages.c
depthcharge-y += boot_policy.c
depthcharge-y += firmware_id.c
depthcharge-y += nvdata.c

ifeq ($(CONFIG_MOCK_TPM),y)
depthcharge-y += mocked_secdata_tpm.c
else
depthcharge-y += secdata_tpm.c
endif

# Where "main" lives.
vboot-y += main.c

VBOOT_BUILD_DIR = $(abspath $(obj)/external/vboot)
VBOOT_LIB = $(VBOOT_BUILD_DIR)/vboot_fw.a
TLCL_LIB = $(VBOOT_BUILD_DIR)/tlcl.a

.PHONY: $(VBOOT_LIB)
.PHONY: $(TLCL_LIB)

# The vboot and TLCL library sources are external to depthcharge.
$(VBOOT_LIB): $(LIBPAYLOAD_DIR)/include/kconfig.h $(KCONFIG_AUTOHEADER)
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
		BOOT_EXTERNAL_ON_DEV=$(CONFIG_BOOT_EXTERNAL_ON_DEV) \
		EC_SLOW_UPDATE=$(CONFIG_EC_SLOW_UPDATE) \
		EC_EFS=$(CONFIG_EC_EFS) \
		DETACHABLE=$(CONFIG_DETACHABLE) \
		PHYSICAL_PRESENCE_KEYBOARD=$(CONFIG_PHYSICAL_PRESENCE_KEYBOARD) \
		CUSTOM_MUSIC=$(CONFIG_CUSTOM_MUSIC) \
		X86_SHA_EXT=$(CONFIG_VBOOT_X86_SHA256_ACCELERATION) \
		UNROLL_LOOPS=1 \
		BUILD=$(VBOOT_BUILD_DIR) \
		V=$(V) \
		fwlib tlcl
endif

$(TLCL_LIB): $(VBOOT_LIB)
