##
## Copyright 2014 Google Inc.
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

depthcharge-$(CONFIG_ARCH_X86) += io.c
depthcharge-$(CONFIG_ARCH_X86) += pci.c
depthcharge-$(CONFIG_KERNEL_LEGACY) += boot.c
depthcharge-y += audio.c
depthcharge-y += cbfs.c
depthcharge-y += command.c
depthcharge-y += console_main.c
depthcharge-y += display.c
depthcharge-y += draw.c
depthcharge-y += i2c.c
depthcharge-y += memory.c
depthcharge-y += printbuf.c
depthcharge-y += spi.c
depthcharge-y += storage.c
depthcharge-y += timer.c

netboot-y += enet.c
