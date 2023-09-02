##
## Copyright 2014 Google LLC
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

dev-$(CONFIG_ARCH_X86) += io.c
dev-$(CONFIG_ARCH_X86) += pci.c
dev-$(CONFIG_KERNEL_LEGACY) += boot.c
dev-y += audio.c
dev-y += cbfs.c
dev-y += command.c
dev-y += console_main.c
ifneq ($(CONFIG_HEADLESS),y)
dev-y += draw.c
endif
dev-y += i2c.c
dev-y += memory.c
dev-y += memtest.c
dev-y += printbuf.c
dev-y += spi.c
dev-y += storage.c
dev-y += timer.c
dev-$(CONFIG_DRIVER_TPM) += tpm.c

netboot-y += net.c
netboot-y += enet.c
