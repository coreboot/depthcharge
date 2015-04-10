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
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
##

subdirs-y := callbacks crossystem stages util

depthcharge-y += stages.c
depthcharge-$(CONFIG_FASTBOOT_MODE) += fastboot.c
depthcharge-y += boot_policy.c
depthcharge-y += firmware_id.c

# Where "main" lives.
readonly-y += ro_main.c
readwrite-y += rw_main.c
unified-y += main.c

VB_LIB = $(obj)/external/vboot/vboot_fw.a

TRAMP_LP = $(obj)/trampoline_libpayload.a
TRAMP_PREFIX = tramp
TRAMP_ENTRY = load_elf
TRAMPOLINE = $(obj)/trampoline.o

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
		BUILD=$(dir $(VB_LIB)) \
		V=$(V) \
		fwlib
endif

.PHONY: $(VB_LIB)

# Trampoline to go from RO depthcharge to RW. Make our own libpayload where only
# a few whitelisted symbols are linkable to avoid accidental binary bloat here.
$(TRAMP_LP): $(LIBPAYLOAD_DIR)/lib/libpayload.a
	@printf "    OBJCOPY    $(subst $(obj)/,,$(@))\n"
	$(Q)$(OBJCOPY) --prefix-symbols=$(TRAMP_PREFIX)_ \
		--remove-section=".reginfo" \
		--keep-global-symbol=$(TRAMP_PREFIX)_memcpy \
		--keep-global-symbol=$(TRAMP_PREFIX)_memset \
		--keep-global-symbol=$(TRAMP_PREFIX)_dcache_clean_all \
		--keep-global-symbol=$(TRAMP_PREFIX)_cache_sync_instructions \
		--prefix-alloc-sections=.$(TRAMP_PREFIX) $(<) $(@)

# The trampoline is linked with normal gcc to avoid linking the full libpayload.
$(TRAMPOLINE): $$(trampoline-objs)
	@printf "    LD         $(subst $(obj)/,,$(@).tmp)\n"
	$(Q)$(CC) $(LINK_FLAGS) -Wl,-relocatable -o $(@).tmp \
		$(trampoline-objs) -nostdinc -nostdlib -static \
		-e $(TRAMP_ENTRY)
	@printf "    OBJCOPY    $(subst $(obj)/,,$(@))\n"
	$(Q)$(OBJCOPY) --prefix-symbols=$(TRAMP_PREFIX)_ $(@).tmp $(@)
