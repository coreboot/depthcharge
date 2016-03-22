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

depthcharge-y += ec.c
depthcharge-$(CONFIG_DRIVER_EC_CROS_I2C) += i2c.c
depthcharge-$(CONFIG_DRIVER_EC_CROS_LPC) += lpc.c
depthcharge-$(CONFIG_DRIVER_EC_CROS_SPI) += spi.c
depthcharge-$(CONFIG_ARCH_X86) += lid_shutdown.c
