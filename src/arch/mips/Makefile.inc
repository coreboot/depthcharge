#
# This file is part of the depthcharge project.
#
# Copyright (C) 2014 Imagination Technologies
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; version 2 of
# the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#

###############################################################################
# MIPS specific options
###############################################################################

depthcharge-y += boot.c
depthcharge-y += physmem.c
depthcharge-$(CONFIG_KERNEL_FIT) += fit.c
