##
## This file is part of the depthcharge project.
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

# Where "main" lives.
readonly-y += ro_main.c
readwrite-y += rw_main.c
unified-y += main.c


DEPTHCHARGE_RO_ELF = $(obj)/depthcharge.ro.elf
DEPTHCHARGE_RO_BIN = $(obj)/depthcharge.ro.bin
DEPTHCHARGE_RW_ELF = $(obj)/depthcharge.rw.elf
DEPTHCHARGE_RW_BIN = $(obj)/depthcharge.rw.bin
DEPTHCHARGE_ELF = $(obj)/depthcharge.elf
DEPTHCHARGE_PAYLOAD = $(obj)/depthcharge.payload

VB_LIB = $(obj)/external/vboot/vboot_fw.a

LIBRARIES = $(VB_LIB)

TRAMP_LP = $(obj)/trampoline_libpayload.a
TRAMP_PREFIX = tramp
TRAMP_ENTRY = load_elf
TRAMPOLINE = $(obj)/trampoline.o

real-target: $(DEPTHCHARGE_RO_ELF) $(DEPTHCHARGE_RO_BIN) \
	$(DEPTHCHARGE_RW_ELF) $(DEPTHCHARGE_RW_BIN) \
	$(DEPTHCHARGE_ELF) $(DEPTHCHARGE_PAYLOAD)

ifeq ($(CONFIG_MOCK_TPM),y)
VBOOT_MOCK_TPM=1
else
VBOOT_MOCK_TPM=$(MOCK_TPM)
endif


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
		MOCK_TPM=$(VBOOT_MOCK_TPM) \
		BUILD=$(dir $(VB_LIB)) \
		V=$(V) \
		fwlib
endif


# Trampoline to go from RO depthcharge to RW. Make our own libpayload where only
# a few whitelisted symbols are linkable to avoid accidental binary bloat here.
$(TRAMP_LP): $(LIBPAYLOAD_DIR)/lib/libpayload.a
	@printf "    OBJCOPY    $(subst $(obj)/,,$(@))\n"
	$(Q)$(OBJCOPY) --prefix-symbols=$(TRAMP_PREFIX)_ \
		--keep-global-symbol=$(TRAMP_PREFIX)_memcpy \
		--keep-global-symbol=$(TRAMP_PREFIX)_memset \
		--keep-global-symbol=$(TRAMP_PREFIX)_dcache_clean_all \
		--keep-global-symbol=$(TRAMP_PREFIX)_icache_invalidate_all \
		--prefix-alloc-sections=.$(TRAMP_PREFIX) $(<) $(@)

# The trampoline is linked with normal gcc to avoid linking the full libpayload.
$(TRAMPOLINE): $$(trampoline-objs)
	@printf "    LD         $(subst $(obj)/,,$(@).tmp)\n"
	$(Q)$(CC) $(LINK_FLAGS) -Wl,-relocatable -o $(@).tmp \
		$(trampoline-objs) -nostdinc -nostdlib -static \
		-e $(TRAMP_ENTRY)
	@printf "    OBJCOPY    $(subst $(obj)/,,$(@))\n"
	$(Q)$(OBJCOPY) --prefix-symbols=$(TRAMP_PREFIX)_ $(@).tmp $(@)


#The read-only version
$(DEPTHCHARGE_RO_ELF): $(src)/.config $$(depthcharge-objs) $$(readonly-objs) \
		$(TRAMPOLINE) $(LIBRARIES) $(TRAMP_LP) prepare
	@printf "    LD         $(subst $(obj)/,,$(@).tmp)\n"
	$(Q)$(XCC) $(LINK_FLAGS) -o $@.tmp $(depthcharge-objs) \
		$(readonly-objs) $(TRAMPOLINE) $(LIBRARIES) $(TRAMP_LP)
	@printf "    OBJCOPY    $(subst $(obj)/,,$(@))\n"
	$(Q)$(OBJCOPY) --remove-section=".shared_data" \
		--remove-section=".cparams" --remove-section=".gbb_copy" \
		--redefine-sym $(TRAMP_PREFIX)_\$$a=\$$a \
		--redefine-sym $(TRAMP_PREFIX)_\$$t=\$$t \
		--redefine-sym $(TRAMP_PREFIX)_\$$d=\$$d \
		$@.tmp $@

$(DEPTHCHARGE_RO_BIN): $(DEPTHCHARGE_RO_ELF)
	@printf "    STRIP      $(subst $(obj)/,,$(@).tmp)\n"
	$(Q)$(STRIP) -o $(@).tmp $(<)
	@printf "    LZMA       $(subst $(obj)/,,$(@))\n"
	$(Q)$(LZMA) --stdout $(@).tmp > $(@)


#The read/write version
$(DEPTHCHARGE_RW_ELF): $(src)/.config $$(depthcharge-objs) $$(readwrite-objs) \
		$(LIBRARIES) prepare
	@printf "    LD         $(subst $(obj)/,,$(@).tmp)\n"
	$(Q)$(XCC) $(LINK_FLAGS) -o $@.tmp $(depthcharge-objs) \
		$(readwrite-objs) $(LIBRARIES)
	@printf "    OBJCOPY    $(subst $(obj)/,,$(@))\n"
	$(Q)$(OBJCOPY) --remove-section=".shared_data" \
		--remove-section=".cparams" --remove-section=".gbb_copy" \
		$@.tmp $@

$(DEPTHCHARGE_RW_BIN): $(DEPTHCHARGE_RW_ELF)
	@printf "    STRIP      $(subst $(obj)/,,$(@).tmp)\n"
	$(Q)$(STRIP) -o $(@).tmp $(<)
	@printf "    LZMA       $(subst $(obj)/,,$(@))\n"
	$(Q)$(LZMA) --stdout $(@).tmp > $(@)

# The unified version
$(DEPTHCHARGE_ELF): $(src)/.config $$(depthcharge-objs) \
		$$(unified-objs) $(LIBRARIES) prepare
	@printf "    LD         $(subst $(obj)/,,$(@).tmp)\n"
	$(Q)$(XCC) $(LINK_FLAGS) -o $@.tmp $(depthcharge-objs) \
		$(unified-objs) $(LIBRARIES)
	@printf "    OBJCOPY    $(subst $(obj)/,,$(@))\n"
	$(Q)$(OBJCOPY) --remove-section=".shared_data" \
		--remove-section=".cparams" --remove-section=".gbb_copy" \
		$@.tmp $@

# Provide a unified deptcharge boot loader in a payload format.
$(DEPTHCHARGE_PAYLOAD): $(DEPTHCHARGE_ELF)
	@printf "    PAYLOAD    $(subst $(obj)/,,$(@))\n"
	$(Q)-rm -f temp.rom temp.bb
	$(Q)dd if=/dev/zero of=temp.bb bs=512 count=1
	$(Q)cbfstool temp.rom create -m $(ARCH) -s 1024K -B temp.bb -a 64
	$(Q)cbfstool temp.rom add-payload -f $< -n dc.elf -c lzma
	$(Q)cbfstool temp.rom extract -n dc.elf -f $@
	$(Q)rm -f temp.rom temp.bb
