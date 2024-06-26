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

depthcharge-y += flag.c
depthcharge-y += memory.c
depthcharge-$(CONFIG_ARCH_ARM_V8) += physmem_64.c
depthcharge-$(CONFIG_ARCH_X86_64) += physmem_64.c
