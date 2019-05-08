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

subdirs-y := arch base board boot debug drivers image net netboot vboot fastboot

DEPTHCHARGE = $(obj)/depthcharge
NETBOOT = $(obj)/netboot
DEV = $(obj)/dev
FASTBOOT = $(obj)/fastboot

DONT_GC_NETBOOT = -Wl,-u,netboot_entry

PREPROCESS_ONLY := -E -P -x assembler-with-cpp -undef

LDSCRIPT_SRC := $(src)/src/image/depthcharge.ldscript.S
LDSCRIPT_OBJ := $(obj)/image/depthcharge.ldscript

# For ldscript generation, include depthcharge config.h
# (KCONFIG_AUTOHEADER), and libpayload config.h (included
# from kconfig.h).  kconfig.h also defines the CONFIG(option)
# macro, which should be used when accessing boolean
# config.h values.
$(LDSCRIPT_OBJ): $(LDSCRIPT_SRC) $(KCONFIG_AUTOHEADER)
	@printf "    LDSCRIPT   $@\n"
	$(HOSTCC) $(PREPROCESS_ONLY) -I $(LIBPAYLOAD_DIR)/include \
		-include $(LIBPAYLOAD_DIR)/include/kconfig.h \
		-include $(KCONFIG_AUTOHEADER) $< -o $@

# Defines rules for the payload executable
# $1 output file base name (will result in $1.elf)
# $2 objects to be linked into the binary
# $3 additional compiler/linker flags to pass
define declare_payload

$1.elf: $2 $$$$(VB_LIB) $(LDSCRIPT_OBJ)
	@printf "    LD         $$(subst $$(obj)/,,$$@.tmp)\n"
	$(Q)$$(XCC) $$(LINK_FLAGS) $3 -o $$@.tmp $$^
	@printf "    OBJCOPY    $$(subst $$(obj)/,,$$@)\n"
	$(Q)$$(OBJCOPY) --remove-section=".shared_data" \
		--remove-section=".cparams" --remove-section=".gbb_copy" \
		$$@.tmp $$@

$(notdir $1): $1.elf
PHONY += $(notdir $1)
endef

$(eval $(call declare_payload,$(DEPTHCHARGE),$$$$(depthcharge-objs) \
	$$$$(vboot-objs),))

$(eval $(call declare_payload,$(NETBOOT),$$$$(depthcharge-objs) \
	$$$$(netboot-objs) $$$$(net-objs) $$$$(uip-objs),))

$(eval $(call declare_payload,$(DEV),$$$$(depthcharge-objs) \
	$$$$(netboot-objs) $$$$(net-objs) $$$$(uip-objs) $$$$(dev-objs) \
	$$$$(vboot-objs),$(DONT_GC_NETBOOT)))

# Rules for creating fastboot-capable depthcharge binary

$(eval $(call declare_payload,$(FASTBOOT),$$$$(depthcharge-objs) \
	$$$$(fastboot-objs) $$$$(vboot-objs),))
