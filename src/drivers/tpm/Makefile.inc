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

subdirs-$(CONFIG_DRIVER_TPM_GOOGLE) += google

ifneq ($(CONFIG_MOCK_TPM),)
depthcharge-$(CONFIG_MOCK_TPM) += mocked_tpm.c
else
depthcharge-$(CONFIG_DRIVER_TPM) += tpm.c
endif
depthcharge-$(CONFIG_DRIVER_TPM_I2C) += i2c.c
depthcharge-$(CONFIG_DRIVER_TPM_LPC) += lpc.c
depthcharge-$(CONFIG_DRIVER_TPM_SLB96) += slb96_i2c.c
