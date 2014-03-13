##
## Copyright 2013 Google Inc.
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

netboot-y += dhcp.c
netboot-y += main.c
netboot-y += params.c
netboot-y += tftp.c

NETBOOT_ELF = $(obj)/netboot.elf
NETBOOT_BIN = $(obj)/netboot.bin
NETBOOT_PAYLOAD = $(obj)/netboot.payload

real-target: $(NETBOOT_ELF) $(NETBOOT_BIN) $(NETBOOT_PAYLOAD)

$(NETBOOT_ELF): $(src)/.config $$(depthcharge-objs) $$(netboot-objs) \
		$$(net-objs) $$(uip-objs) prepare $$(VB_LIB)
	@printf "    LD         $(subst $(obj)/,,$(@).tmp)\n"
	$(Q)$(XCC) $(LINK_FLAGS) -o $@.tmp $(depthcharge-objs) \
		$(netboot-objs) $(net-objs) $(uip-objs) $(VB_LIB)
	@printf "    OBJCOPY    $(subst $(obj)/,,$(@))\n"
	$(Q)$(OBJCOPY) --remove-section=".shared_data" \
		--remove-section=".cparams" --remove-section=".gbb_copy" \
		$@.tmp $@

$(NETBOOT_BIN): $(NETBOOT_ELF)
	@printf "    STRIP      $(subst $(obj)/,,$(@).tmp)\n"
	$(Q)$(STRIP) -o $(@).tmp $(<)
	@printf "    LZMA       $(subst $(obj)/,,$(@))\n"
	$(Q)$(LZMA) --stdout $(@).tmp > $(@)

# Provide a netboot loader in a payload format.
$(NETBOOT_PAYLOAD): $(NETBOOT_ELF)
	@printf "    PAYLOAD    $(subst $(obj)/,,$(@))\n"
	$(Q)-rm -f temp.nrom temp.nbb
	$(Q)dd if=/dev/zero of=temp.nbb bs=512 count=1
	$(Q)cbfstool temp.nrom create -m $(ARCH) -s 1024K -B temp.nbb -a 64
	$(Q)cbfstool temp.nrom add-payload -f $< -n nb.elf -c lzma
	$(Q)cbfstool temp.nrom extract -n nb.elf -f $@
	$(Q)rm -f temp.nrom temp.nbb
