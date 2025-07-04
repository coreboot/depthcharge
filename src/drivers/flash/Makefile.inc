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

depthcharge-$(CONFIG_DRIVER_FLASH) += flash.c block_flash.c
depthcharge-$(CONFIG_DRIVER_FLASH_FAST_SPI) += fast_spi.c
depthcharge-$(CONFIG_DRIVER_FLASH_MEMMAPPED) += memmapped.c
depthcharge-$(CONFIG_DRIVER_FLASH_SPI) += spi.c
depthcharge-$(CONFIG_DRIVER_CBFS_FLASH) += cbfs.c
depthcharge-$(CONFIG_DRIVER_FLASH_MTK_SNFC) += mtk_snfc.c
