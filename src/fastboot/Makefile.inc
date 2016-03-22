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

depthcharge-y += backend.c
depthcharge-y += capabilities.c
depthcharge-$(CONFIG_DRIVER_EC_CROS) += ec.c
depthcharge-y += fastboot.c
depthcharge-y += print.c
depthcharge-y += udc.c
