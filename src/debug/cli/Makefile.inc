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

depthcharge-$(CONFIG_ARCH_X86) += io.c
depthcharge-$(CONFIG_ARCH_X86) += pci.c
depthcharge-$(CONFIG_KERNEL_LEGACY) += boot.c
depthcharge-y += audio.c
depthcharge-y += cbfs.c
depthcharge-y += command.c
depthcharge-y += console_main.c
depthcharge-y += display.c
ifneq ($(CONFIG_HEADLESS),y)
depthcharge-y += draw.c
endif
depthcharge-y += i2c.c
depthcharge-y += memory.c
depthcharge-y += memtest.c
depthcharge-y += printbuf.c
depthcharge-y += spi.c
depthcharge-y += storage.c
depthcharge-y += timer.c
depthcharge-y += tpm.c

netboot-y += net.c
netboot-y += enet.c
