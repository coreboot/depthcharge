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

depthcharge-$(CONFIG_DRIVER_TPM) += tpm.c
depthcharge-$(CONFIG_DRIVER_TPM) += tpm_utils.c
depthcharge-$(CONFIG_DRIVER_TPM_CR50_I2C) += cr50_i2c.c
depthcharge-$(CONFIG_DRIVER_TPM_I2C) += i2c.c
depthcharge-$(CONFIG_DRIVER_TPM_LPC) += lpc.c
depthcharge-$(CONFIG_DRIVER_TPM_SLB9635_I2C) += slb9635_i2c.c
depthcharge-$(CONFIG_DRIVER_TPM_SPI) += spi.c
depthcharge-$(CONFIG_DRIVER_TPM_CR50_REC) += cr50_rec_switch.c
depthcharge-$(CONFIG_TPM_DEBUG_EXTENSIONS) += tpm_state.c
