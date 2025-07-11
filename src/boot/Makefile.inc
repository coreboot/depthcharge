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

subdirs-y := zbi
depthcharge-y += commandline.c payload.c
depthcharge-$(CONFIG_BOOTCONFIG) += bootconfig.c
depthcharge-$(CONFIG_BOOTCONFIG) += android_bootconfig_params.c
depthcharge-$(CONFIG_KERNEL_DUMMY) += dummy.c
depthcharge-$(CONFIG_KERNEL_FIT) += fit.c
depthcharge-$(CONFIG_ARCH_ARM) += coreboot.c
depthcharge-$(CONFIG_KERNEL_FIT) += ramoops.c
depthcharge-$(CONFIG_KERNEL_FIT) += memchipinfo.c
depthcharge-$(CONFIG_KERNEL_LEGACY) += legacy_boot.c
depthcharge-$(CONFIG_KERNEL_LEGACY) += crc32.c
depthcharge-$(CONFIG_KERNEL_MULTIBOOT) += multiboot.c
depthcharge-$(CONFIG_KERNEL_MULTIBOOT_ZBI) += zbi.c
depthcharge-$(CONFIG_ANDROID_PVMFW) += android_pvmfw.c
