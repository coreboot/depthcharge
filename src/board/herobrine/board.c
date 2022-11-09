/*
* This file is part of the coreboot project.
 *
 * Copyright (C) 2021, The Linux Foundation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <assert.h>
#include <libpayload.h>
#include "base/init_funcs.h"
#include "drivers/gpio/gpio.h"
#include "vboot/util/flag.h"
#include "boot/fit.h"
#include "drivers/bus/i2c/qcom_qupv3_i2c.h"
#include "drivers/storage/sdhci_msm.h"
#include "drivers/gpio/gpio.h"
#include "drivers/gpio/qcom_gpio.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/psci.h"
#include "drivers/tpm/google/i2c.h"
#include "drivers/soc/qcom_sd_tray.h"
#include "drivers/tpm/google/i2c.h"
#include "drivers/soc/qcom_sd_tray.h"
#include "drivers/bus/usb/usb.h"

#define SDC1_HC_BASE          0x007C4000

#define SDC2_HC_BASE 0x08804000

static const VpdDeviceTreeMap vpd_dt_map[] = {
	{ "bluetooth_mac0", "bluetooth0/local-bd-address" },
	{ "wifi_mac0", "wifi0/local-mac-address" },
	{}
};

static int herobrine_tpm_irq_status(void)
{
	static GpioOps *tpm_int = NULL;

	if (!tpm_int)
		tpm_int = sysinfo_lookup_gpio("TPM interrupt", 1,
					new_gpio_latched_from_coreboot);
	return gpio_get(tpm_int);
}

static int board_setup(void)
{
	/* stub out required GPIOs for vboot */
	flag_replace(FLAG_LIDSW, new_gpio_high());
	flag_replace(FLAG_PWRSW, new_gpio_low());

	power_set_ops(&psci_power_ops);

	/* Support USB3.0 XHCI controller in firmware. */
	UsbHostController *usb_host = new_usb_hc(XHCI, 0xa600000);
	list_insert_after(&usb_host->list_node, &usb_host_controllers);

	dt_register_vpd_mac_fixup(vpd_dt_map);

	/*eMMC support */
	u32 emmc_platfm_flags = SDHCI_PLATFORM_EMMC_1V8_POWER |
				SDHCI_PLATFORM_EMMC_HARDWIRED_VCC |
				SDHCI_PLATFORM_NO_EMMC_HS200 |
				SDHCI_PLATFORM_SUPPORTS_HS400ES;
	SdhciHost *emmc;
	emmc = new_sdhci_msm_host(SDC1_HC_BASE,
			emmc_platfm_flags,
			384*MHz,
			NULL);
	list_insert_after(&emmc->mmc_ctrlr.ctrlr.list_node,
		&fixed_block_dev_controllers);

	/* SD card support */
	GpioOps *sd_cd = sysinfo_lookup_gpio("SD card detect", 1,
					new_gpio_input_from_coreboot);

	SdhciHost *sd = new_sdhci_msm_host(SDC2_HC_BASE,
					SDHCI_PLATFORM_REMOVABLE,
					50*MHz, sd_cd);
	list_insert_after(&sd->mmc_ctrlr.ctrlr.list_node,
				&removable_block_dev_controllers);

	if (CONFIG(DRIVER_TPM_CR50_I2C)) {
		I2cOps *tpm_i2c = &new_qup_i2c(0xA98000)->ops;
		Cr50I2c *tpm_bus = new_cr50_i2c(tpm_i2c, 0x50,
						herobrine_tpm_irq_status);
		tpm_set_ops(&tpm_bus->base.ops);
	}

	if (CONFIG(DRIVER_TPM_I2C))
		tpm_set_ops(&new_cr50_i2c(&new_qup_i2c(0xA98000)->ops, 0x50,
					  herobrine_tpm_irq_status)->base.ops);
	else if (CONFIG(DRIVER_TPM_SPI))
		tpm_set_ops(&new_tpm_spi(&new_qup_spi(0xA98000)->ops,
					 herobrine_tpm_irq_status)->ops);

	/* Audio support */
	amp_enable = sysinfo_lookup_gpio("speaker enable", 1,
				new_gpio_output_from_coreboot);
	speaker_amp = new_gpio_amp_codec(amp_enable);
	soundq = new_lpass_i2s(48000, 2, 16, LPASS_SECONDARY, 0x2C00000);
	i2s_source = new_i2s_source(&soundq->ops, 48000, 2, 0x500);
	sound = new_sound_route(&i2s_source->ops);
	list_insert_after(&speaker_amp->component.list_node,
			&sound->components);

	sound_set_ops(&sound->ops);

	return 0;
}

INIT_FUNC(board_setup);
