# SPDX-License-Identifier: GPL-2.0

depthcharge-y += tpm.c
depthcharge-y += tpm_state.c
depthcharge-$(CONFIG_DRIVER_TPM_I2C) += i2c.c
depthcharge-$(CONFIG_DRIVER_TPM_SPI) += spi.c
depthcharge-$(CONFIG_TPM_GOOGLE_SWITCHES) += switches.c
