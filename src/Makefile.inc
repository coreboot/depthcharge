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

subdirs-y := arch base board boot debug drivers image net netboot vboot

DEPTHCHARGE = $(obj)/depthcharge
NETBOOT = $(obj)/netboot

# Defines rules to link, strip and compress a binary
# $1 output file base name (will result in $1.elf and $1.bin)
# $2 prerequisite objects to build and link in
# $3 additional compiler/linker flags to pass
define declare_bin
$1.elf: $2
	@printf "    LD         $$(subst $$(obj)/,,$$@.tmp)\n"
	$$(Q)$$(XCC) $$(LINK_FLAGS) $3 -o $$@.tmp $$^
	@printf "    OBJCOPY    $$(subst $$(obj)/,,$$@)\n"
	$$(Q)$$(OBJCOPY) --remove-section=".shared_data" \
		--remove-section=".cparams" --remove-section=".gbb_copy" \
		--redefine-sym $$(TRAMP_PREFIX)_\$$$$a=\$$$$a \
		--redefine-sym $$(TRAMP_PREFIX)_\$$$$t=\$$$$t \
		--redefine-sym $$(TRAMP_PREFIX)_\$$$$d=\$$$$d \
		$$@.tmp $$@

$1.bin: $1.elf
	@printf "    STRIP      $$(subst $$(obj)/,,$$@.tmp)\n"
	$$(Q)$$(STRIP) -o $$@.tmp $$<
	@printf "    LZMA       $$(subst $$(obj)/,,$$@)\n"
	$$(Q)$$(LZMA) --stdout $$@.tmp > $$@
endef

# Defines rules for a legacy binaries (depthcharge RO and RW with trampoline)
# $1 output file base name (will result in $1.ro.elf, $1.rw.bin)
# $2 common objects to be linked into  both stages
# $3 object containing entry point (main) for RO
# $4 object containing entry point (main) for RW
# $5 additional compiler/linker flags to pass
define declare_ro_rw

$(eval $(call declare_bin,$1.ro,$2 $3 $$$$(VB_LIB) \
				$$$$(TRAMPOLINE) $$$$(TRAMP_LP),$5))

$(eval $(call declare_bin,$1.rw,$2 $4 $$$$(VB_LIB),$5))

$1_ro_rw: $1.ro.bin $1.rw.bin
PHONY += $1_ro_rw
endef

# Defines rules for a unified (EFS) binary
# $1 output file base name (will result in $1.elf)
# $2 objects to be linked into the binary
# $3 additional compiler/linker flags to pass
define declare_unified

$(eval $(call declare_bin,$1,$2 $$$$(VB_LIB),$3))

$1.payload: $1.elf
	@printf "    PAYLOAD    $$(subst $$(obj)/,,$$@)\n"
	$$(Q)-rm -f $1.rom $1.bb
	$$(Q)dd if=/dev/zero of=$1.bb bs=512 count=1
	$$(Q)cbfstool $1.rom create -m $$(ARCH) -s 1024K -B $1.bb -a 64
	$$(Q)cbfstool $1.rom add-payload -f $$< -n dc.elf -c lzma
	$$(Q)cbfstool $1.rom extract -n dc.elf -f $$@
	$$(Q)rm -f $1.rom $1.bb

$1_unified: $1.bin $1.payload
PHONY += $1_unified
endef


$(eval $(call declare_ro_rw,$(DEPTHCHARGE),$$$$(depthcharge-objs), \
	$$$$(readonly-objs),$$$$(readwrite-objs),))
$(eval $(call declare_unified,$(DEPTHCHARGE),$$$$(depthcharge-objs) \
	$$$$(unified-objs),))

$(eval $(call declare_unified,$(NETBOOT),$$$$(depthcharge-objs) \
	$$$$(netboot-objs) $$$$(net-objs) $$$$(uip-objs),))

real-target: $(NETBOOT)_unified
real-target: $(DEPTHCHARGE)_unified
# TODO(dnojiri): Get rid of this once we decide to support only
# EFS/depthcharge and non-EFS/u-boot.
ifneq ($(CONFIG_ARCH_ARM_V8),y)
real-target: $(DEPTHCHARGE)_ro_rw
endif
