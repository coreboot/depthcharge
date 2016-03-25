##
## This file is part of the depthcharge project.
##
## Copyright 2015 Google Inc.
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

fastboot-y += backend.c
fastboot-y += capabilities.c
fastboot-$(CONFIG_DRIVER_EC_CROS) += ec.c
fastboot-y += fastboot.c
fastboot-y += print.c
fastboot-y += udc.c
