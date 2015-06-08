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

depthcharge-y += fmap.c
depthcharge-y += index.c
readonly-y += startrw.c

trampoline-y += load_elf.c

# These options are passed from .config to the linker script. When making
# changes, update Kconfig as well.
link_config_options := \
	KERNEL_START \
	KERNEL_SIZE \
	BASE_ADDRESS \
	HEAP_SIZE \
	STACK_SIZE \
	GBB_COPY_SIZE \
	FASTBOOT_HEAP_SIZE \
