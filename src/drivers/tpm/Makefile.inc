##
## This file is part of the depthcharge project.
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

depthcharge-$(CONFIG_DRIVER_TPM_DUMMY) += dummy.c
depthcharge-$(CONFIG_DRIVER_TPM_LPC) += lpctpm.c
depthcharge-$(CONFIG_DRIVER_TPM_I2C) += tis_i2c.c
depthcharge-$(CONFIG_DRIVER_TPM_I2C) += slb9635_i2c/tpm.c
depthcharge-$(CONFIG_DRIVER_TPM_I2C) += slb9635_i2c/tpm_tis_i2c.c
