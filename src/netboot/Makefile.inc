##
## This file is part of the depthcharge project.
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

netboot-y += appcall.c
netboot-y += bootp.c
netboot-y += main.c
netboot-y += output.c
netboot-y += tftp.c

NETBOOT_ELF = $(obj)/netboot.elf

real-target: $(NETBOOT_ELF)

$(NETBOOT_ELF): $(src)/.config $$(depthcharge-objs) $$(netboot-objs) \
		$$(uip-objs) prepare
	@printf "    LD         $(subst $(obj)/,,$(@).tmp)\n"
	$(Q)$(XCC) $(LINK_FLAGS) -o $@.tmp $(depthcharge-objs) \
		$(netboot-objs) $(uip-objs)
	@printf "    OBJCOPY    $(subst $(obj)/,,$(@))\n"
	$(Q)$(OBJCOPY) --remove-section=".shared_data" \
		--remove-section=".cparams" --remove-section=".gbb_copy" \
		$@.tmp $@
