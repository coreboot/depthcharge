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

depthcharge-y += debug.c
depthcharge-y += disk.c
depthcharge-y += misc.c
depthcharge-y += time.c
depthcharge-y += tpm.c

ifeq ($(CONFIG_EC_VBOOT_SUPPORT),y)
depthcharge-y += ec.c
else
depthcharge-y += ec_stub.c
endif

ifeq ($(CONFIG_AUXFW_SOFTWARE_SYNC),y)
depthcharge-y += auxfw.c
else
depthcharge-y += auxfw_stub.c
endif
