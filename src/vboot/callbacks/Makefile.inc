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

depthcharge-y += cgptlib.c
depthcharge-y += debug.c
depthcharge-y += disk.c
ifneq ($(CONFIG_HEADLESS),y)
depthcharge-y += display.c
else
depthcharge-y += headless_stub.c
endif
depthcharge-y += keyboard.c
depthcharge-y += legacy.c
depthcharge-y += load_firmware.c
depthcharge-y += memory.c
depthcharge-y += misc.c
depthcharge-$(CONFIG_NV_STORAGE_CMOS) += nvstorage_cmos.c
depthcharge-$(CONFIG_NV_STORAGE_CROS_EC) += nvstorage_cros_ec.c
depthcharge-$(CONFIG_NV_STORAGE_DISK) += nvstorage_disk.c
depthcharge-$(CONFIG_NV_STORAGE_FAKE) += nvstorage_fake.c
depthcharge-$(CONFIG_NV_STORAGE_FLASH) += nvstorage_flash.c
depthcharge-y += switches.c
depthcharge-y += time.c
depthcharge-y += tpm.c

ifeq ($(CONFIG_EC_SOFTWARE_SYNC),y)
depthcharge-y += ec.c
else
depthcharge-y += ec_stub.c
endif
