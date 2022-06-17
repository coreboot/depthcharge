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
#include "drivers/bus/spi/qcom_qupv3_spi.h"
#include "drivers/flash/spi.h"
#include "drivers/gpio/gpio.h"
#include "drivers/tpm/google/spi.h"
#include "vboot/util/flag.h"
#include "boot/fit.h"
#include "drivers/bus/i2c/qcom_qupv3_i2c.h"
#include "drivers/bus/spi/qcom_qupv3_spi.h"
#include "drivers/ec/cros/spi.h"
#include "drivers/flash/spi.h"
#include "drivers/storage/sdhci_msm.h"
#include "drivers/gpio/gpio.h"
#include "drivers/gpio/qcom_gpio.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/psci.h"
#include "drivers/tpm/google/i2c.h"
#include "drivers/soc/qcom_sd_tray.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/bus/spi/qcom_qspi.h"
#include "drivers/gpio/sc7280.h"
#include "drivers/bus/i2s/qcom_lpass.h"
#include "drivers/sound/i2s.h"
#include "drivers/sound/route.h"
#include "drivers/sound/sound.h"
#include "drivers/sound/gpio_amp.h"

#define PMIC_CORE_REGISTERS_ADDR 0x0C600000
#define PMIC_REG_CHAN0_ADDR 0x0C440900
#define PMIC_REG_LAST_CHAN_ADDR 0x1100
#define PMIC_REG_FIRST_CHAN_ADDR 0x900

#define LDO09_EN_CTL 0xC946
#define SID 0x02

#define REG_ENABLE_ADDR ((SID << 16) | LDO09_EN_CTL)
#define ENABLE_REG_DATA 0x80

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

static const DtPathMap xo_cal_map[] = {
	{1, "/soc@0/wifi@a000000", "xo-cal-data" },
	{}
};

static int fix_device_tree(DeviceTreeFixup *fixup, DeviceTree *tree)
{
	int rv = 0;
	rv |= dt_set_xo_cal_data(tree, xo_cal_map);
	return rv;
}

static DeviceTreeFixup ipq_enet_fixup = {
	.fixup = fix_device_tree
};

static int board_setup(void)
{
	GpioOps *amp_enable;
	GpioAmpCodec *speaker_amp;
	LpassI2s *soundq;
	I2sSource *i2s_source;
	SoundRoute *sound;

	/*
	 * flag_fetch() will die if it encounters a flag that is not registered,
	 * so we still need to register lid switch even if we don't have a lid.
	 */
	if (CONFIG(DRIVER_EC_CROS))
		flag_replace(FLAG_LIDSW, cros_ec_lid_switch_flag());
	else
		flag_replace(FLAG_LIDSW, new_gpio_high());

	/* stub out required GPIOs for vboot */
	flag_replace(FLAG_PWRSW, new_gpio_low());

	list_insert_after(&ipq_enet_fixup.list_node, &device_tree_fixups);
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
	QcomSpmi *pmic_spmi = new_qcom_spmi(PMIC_CORE_REGISTERS_ADDR,
					    PMIC_REG_CHAN0_ADDR,
					    PMIC_REG_LAST_CHAN_ADDR - PMIC_REG_FIRST_CHAN_ADDR);
	GpioOps *sd_cd = sysinfo_lookup_gpio("SD card detect", 1,
					     new_gpio_input_from_coreboot);

	GpioOps *cd_wrapper = new_qcom_sd_tray_cd_wrapper(sd_cd, pmic_spmi,
							  REG_ENABLE_ADDR,
							  ENABLE_REG_DATA);
	SdhciHost *sd = new_sdhci_msm_host(SDC2_HC_BASE,
					   SDHCI_PLATFORM_REMOVABLE,
					   50*MHz, cd_wrapper);
	list_insert_after(&sd->mmc_ctrlr.ctrlr.list_node,
				&removable_block_dev_controllers);

	/* SPI-NOR Flash driver - GPIO_15 as Chip Select */
	QcomQspi *spi_flash = new_qcom_qspi(0x088DC000, (GpioOps *)&new_gpio_output(GPIO(15))->ops);
	SpiFlash *flash = new_spi_flash(&spi_flash->ops);
	flash_set_ops(&flash->ops);

	if (CONFIG(DRIVER_EC_CROS)) {
		SpiOps *ec_spi = &new_qup_spi(0xA88000)->ops;
		CrosEcBusOps *ec_bus = &new_cros_ec_spi_bus(ec_spi)->ops;
		GpioOps *ec_int = sysinfo_lookup_gpio("EC interrupt", 1,
					new_gpio_input_from_coreboot);
		CrosEc *ec = new_cros_ec(ec_bus, ec_int);
		register_vboot_ec(&ec->vboot);
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
