##
## This file is part of the depthcharge project.
##
## Copyright (c) 2012 The Chromium OS Authors.
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

DEPTHCHARGE_RO_ELF = $(obj)/depthcharge.ro.elf
DEPTHCHARGE_RW_ELF = $(obj)/depthcharge.rw.elf
DEPTHCHARGE_RW_BIN = $(obj)/depthcharge.rw.bin
LIBRARIES = $(VB_LD_DIR)/vboot_fw.a

LIBGCC = $(shell $(XCC) -print-libgcc-file-name)
TRAMP_LIBGCC = $(obj)/trampoline_$(notdir $(LIBGCC))
TRAMP_PREFIX = .tramp
TRAMPOLINE = $(obj)/trampoline.o


real-target: $(DEPTHCHARGE_RO_ELF) $(DEPTHCHARGE_RO_BIN) \
	$(DEPTHCHARGE_RW_ELF) $(DEPTHCHARGE_RW_BIN)

$(TRAMP_LIBGCC): $(LIBGCC)
	$(Q)printf "    OBJCOPY    $(subst $(obj)/,,$(@))\n"
	$(Q)$(OBJCOPY) --prefix-symbols=$(TRAMP_PREFIX) $(<) $(@)

# The trampoline is linked with normal gcc to avoid linking in libpayload.
$(TRAMPOLINE): $$(trampoline-objs)
	$(Q)printf "    LD         $(subst $(obj)/,,$(@).tmp)\n"
	$(Q)$(CC) $(LINK_FLAGS) -Wl,-relocatable -o $(@).tmp \
		$(trampoline-objs) -nostdinc -nostdlib -static
	$(Q)printf "    OBJCOPY    $(subst $(obj)/,,$(@))\n"
	$(Q)$(OBJCOPY) --prefix-symbols=$(TRAMP_PREFIX) $(@).tmp $(@)


#The read-only version
$(DEPTHCHARGE_RO_ELF): $(src)/.config $$(depthcharge-objs) $$(readonly-objs) \
		$(TRAMPOLINE) $(LIBRARIES) $(TRAMP_LIBGCC) prepare
	$(Q)printf "    LD         $(subst $(obj)/,,$(@).tmp)\n"
	$(Q)$(XCC) $(LINK_FLAGS) -o $@.tmp $(depthcharge-objs) \
		$(readonly-objs) $(TRAMPOLINE) $(LIBRARIES) $(TRAMP_LIBGCC)
	$(Q)printf "    OBJCOPY    $(subst $(obj)/,,$(@))\n"
	$(Q)$(OBJCOPY) --remove-section=".shared_data" \
		--remove-section=".cparams" $@.tmp $@

$(DEPTHCHARGE_RO_BIN): $(DEPTHCHARGE_RW_ELF)
	$(Q)printf "    STRIP      $(subst $(obj)/,,$(@).tmp)\n"
	$(Q)$(STRIP) -o $(@).tmp $(<)
	$(Q)printf "    LZMA       $(subst $(obj)/,,$(@))\n"
	$(Q)$(LZMA) --stdout $(@).tmp > $(@)


#The read/write version
$(DEPTHCHARGE_RW_ELF): $(src)/.config $$(depthcharge-objs) $$(readwrite-objs) \
		$(LIBRARIES) prepare
	$(Q)printf "    LD         $(subst $(obj)/,,$(@).tmp)\n"
	$(Q)$(XCC) $(LINK_FLAGS) -o $@.tmp $(depthcharge-objs) \
		$(readwrite-objs) $(LIBRARIES)
	$(Q)printf "    OBJCOPY    $(subst $(obj)/,,$(@))\n"
	$(Q)$(OBJCOPY) --remove-section=".shared_data" \
		--remove-section=".cparams" $@.tmp $@

$(DEPTHCHARGE_RW_BIN): $(DEPTHCHARGE_RW_ELF)
	$(Q)printf "    STRIP      $(subst $(obj)/,,$(@).tmp)\n"
	$(Q)$(STRIP) -o $(@).tmp $(<)
	$(Q)printf "    LZMA       $(subst $(obj)/,,$(@))\n"
	$(Q)$(LZMA) --stdout $(@).tmp > $(@)

classes-y := depthcharge trampoline readwrite readonly

subdirs-y := src/
