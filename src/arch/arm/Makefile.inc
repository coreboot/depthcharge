##
## Copyright 2013 Google LLC
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

ifeq ($(CONFIG_ARCH_ARM_V8),y)
depthcharge-y += boot64.c boot64_asm.S smc.S
else
depthcharge-y += boot_asm.S physmem.c boot.c
endif
depthcharge-$(CONFIG_KERNEL_FIT) += fit.c
depthcharge-y += gcc.c
