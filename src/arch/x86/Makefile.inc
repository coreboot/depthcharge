##
## Copyright 2012 Google LLC
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

depthcharge-y += boot.c
depthcharge-$(CONFIG_ARCH_X86_32) += boot_asm.S
depthcharge-$(CONFIG_ARCH_X86_64) += boot_asm64.S
depthcharge-$(CONFIG_ARCH_X86_32) += gcc.c
depthcharge-$(CONFIG_ARCH_X86_32) += physmem.c
depthcharge-$(CONFIG_KERNEL_ZIMAGE) += zimage.c
