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

depthcharge-y += commandline.c
depthcharge-$(CONFIG_KERNEL_DUMMY) += dummy.c
depthcharge-$(CONFIG_KERNEL_FIT) += fit.c
depthcharge-$(CONFIG_ARCH_ARM) += coreboot.c
depthcharge-$(CONFIG_KERNEL_FIT) += ramoops.c
depthcharge-$(CONFIG_KERNEL_LEGACY) += legacy_boot.c
depthcharge-$(CONFIG_KERNEL_LEGACY) += crc32.c
depthcharge-$(CONFIG_ANDROID_DT_FIXUP) += android_dt.c
depthcharge-$(CONFIG_BCB_SUPPORT) += bcb.c
