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

subdirs-$(CONFIG_DRIVER_EC_CROS) += cros
subdirs-$(CONFIG_DRIVER_EC_ANX3429) += anx3429
subdirs-$(CONFIG_DRIVER_EC_ANX7688) += anx7688
subdirs-$(CONFIG_DRIVER_EC_CCGXXF) += ccgxxf
subdirs-$(CONFIG_DRIVER_EC_PS8751) += ps8751
subdirs-$(CONFIG_DRIVER_EC_WILCO) += wilco

depthcharge-y += vboot_ec.c
depthcharge-y += vboot_auxfw.c
