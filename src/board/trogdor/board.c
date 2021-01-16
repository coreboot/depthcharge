/*
* This file is part of the coreboot project.
 *
 * Copyright (C) 2019-20, The Linux Foundation.  All rights reserved.
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
#include "drivers/bus/spi/qcom_qupv3_spi.h"
#include "drivers/ec/cros/spi.h"
#include "drivers/power/psci.h"
#include "drivers/flash/spi.h"
#include "drivers/bus/spi/qspi_sc7180.h"
#include "drivers/storage/sdhci_msm.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/gpio/sc7180.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/tpm/spi.h"
#include "drivers/sound/max98357a.h"
#include "drivers/bus/i2s/sc7180.h"
#include "drivers/sound/i2s.h"
#include "drivers/sound/route.h"
#include "drivers/sound/sound.h"
#include "drivers/video/ec_pwm_backlight.h"
#include "drivers/video/display.h"
#include "drivers/video/sc7180.h"

#define SDC1_HC_BASE          0x7C4000
#define SDC1_TLMM_CFG_ADDR    0x3D7A000
#define SDC2_TLMM_CFG_ADDR 0x3D7B000
#define SDC2_HC_BASE 0x08804000

static const VpdDeviceTreeMap vpd_dt_map[] = {
	{ "bluetooth_mac0", "bluetooth0/local-bd-address" },
	{ "wifi_mac0", "wifi0/local-mac-address" },
	{}
};

static int trogdor_tpm_irq_status(void)
{
	static GpioOps *tpm_int = NULL;

	if (!tpm_int)
		tpm_int = sysinfo_lookup_gpio("TPM interrupt", 1,
					new_sc7180_gpio_latched_from_coreboot);
	return gpio_get(tpm_int);
}

static int init_display_ops(void)
{
	if (lib_sysinfo.framebuffer.physical_address == 0) {
		printf("%s: No framebuffer provided by coreboot\n", __func__);
		return -1;
	}

	/* This just needs to be turned on >80ms after eDP intialization, so
	   just do it here rather than passing through the DisplayOps stack. */
	GpioOps *bl_en = sysinfo_lookup_gpio("backlight", 1,
					new_sc7180_gpio_output_from_coreboot);
	gpio_set(bl_en, 1);

	GpioOps *backlight = NULL;

	if (CONFIG(DRIVER_VIDEO_EC_PWM_BACKLIGHT))
		backlight = new_ec_pwm_backlight();

	display_set_ops(new_sc7180_display(backlight));

	return 0;
}

static int board_setup(void)
{
	sysinfo_install_flags(new_sc7180_gpio_input_from_coreboot);
	flag_replace(FLAG_PWRSW, new_gpio_low());	/* handled by EC */

	if (CONFIG(DRIVER_EC_CROS))
		flag_replace(FLAG_LIDSW, cros_ec_lid_switch_flag());
	else
		flag_replace(FLAG_LIDSW, new_gpio_high());

	power_set_ops(&psci_power_ops);

	struct cb_mainboard *mainboard =
		phys_to_virt(lib_sysinfo.cb_mainboard);
	if (!strcmp(cb_mb_part_string(mainboard), "Bubs"))
		fit_add_compat("qcom,sc7180-idp");

	dt_register_vpd_mac_fixup(vpd_dt_map);

	/* Support USB3.0 XHCI controller in firmware. */
	UsbHostController *usb_host = new_usb_hc(XHCI, 0xa600000);
	list_insert_after(&usb_host->list_node, &usb_host_controllers);

	/*eMMC support */
	u32 emmc_platfm_flags = SDHCI_PLATFORM_EMMC_1V8_POWER |
				SDHCI_PLATFORM_NO_EMMC_HS200 |
				SDHCI_PLATFORM_SUPPORTS_HS400ES;

	/* Some specific board/revision combinations use off-AVL eMMC parts
	   that do not support HS400-ES. */
	if ((lib_sysinfo.board_id == 1 &&
	     !strcmp(cb_mb_part_string(mainboard), "Trogdor")) ||
	    (lib_sysinfo.board_id == 2 &&
	     !strcmp(cb_mb_part_string(mainboard), "Lazor")) ||
	    (lib_sysinfo.board_id < 2 &&
	     !strcmp(cb_mb_part_string(mainboard), "Pompom")))
		emmc_platfm_flags &= ~SDHCI_PLATFORM_SUPPORTS_HS400ES;

	SdhciHost *emmc = new_sdhci_msm_host(SDC1_HC_BASE,
					     emmc_platfm_flags,
					     384*MHz,
					     SDC1_TLMM_CFG_ADDR, NULL);

	list_insert_after(&emmc->mmc_ctrlr.ctrlr.list_node,
			&fixed_block_dev_controllers);

	/* SD card support */
	GpioOps *sd_cd = sysinfo_lookup_gpio("SD card detect", 1,
					new_sc7180_gpio_input_from_coreboot);
	SdhciHost *sd = new_sdhci_msm_host(SDC2_HC_BASE,
					   SDHCI_PLATFORM_REMOVABLE,
					   50*MHz, SDC2_TLMM_CFG_ADDR, sd_cd);
	list_insert_after(&sd->mmc_ctrlr.ctrlr.list_node,
			  &removable_block_dev_controllers);

	SpiOps *spi_qup0se0 = &new_sc7180_Qup_spi(0x880000)->ops;
	SpiOps *spi_qup1se0 = &new_sc7180_Qup_spi(0xa80000)->ops;
	SpiOps *ec_spi = spi_qup1se0;
	SpiOps *tpm_spi = spi_qup0se0;

	/* Trogdor rev0 had EC and TPM QUP swapped. */
	if (lib_sysinfo.board_id < 1 &&
	    !strcmp(cb_mb_part_string(mainboard), "Trogdor")) {
		ec_spi = spi_qup0se0;
		tpm_spi = spi_qup1se0;
	}

	if (CONFIG(DRIVER_TPM_SPI))
		tpm_set_ops(&new_tpm_spi(tpm_spi, trogdor_tpm_irq_status)->ops);

	if (CONFIG(DRIVER_EC_CROS)) {
		CrosEcBusOps *ec_bus = &new_cros_ec_spi_bus(ec_spi)->ops;
		GpioOps *ec_int = sysinfo_lookup_gpio("EC interrupt", 1,
					new_sc7180_gpio_input_from_coreboot);
		CrosEc *ec = new_cros_ec(ec_bus, ec_int);
		register_vboot_ec(&ec->vboot);
	}

	/* SPI-NOR Flash driver */
	Sc7180Qspi *spi_flash = new_sc7180_qspi(0x088DC000);
	SpiFlash *flash = new_spi_flash(&spi_flash->ops);
	flash_set_ops(&flash->ops);

	GpioOps *amp_enable = sysinfo_lookup_gpio("speaker enable", 1,
				new_sc7180_gpio_output_from_coreboot);
	max98357aCodec *speaker_amp = new_max98357a_codec(amp_enable);

	Sc7180I2s *soundq = new_sc7180_i2s(48000, 2, 16, LPASS_SECONDARY,
				           0x62000000);
	I2sSource *i2s_source = new_i2s_source(&soundq->ops, 48000, 2, 0x500);
	SoundRoute *sound = new_sound_route(&i2s_source->ops);
	list_insert_after(&speaker_amp->component.list_node,
					  &sound->components);

	sound_set_ops(&sound->ops);

	if (CONFIG(DRIVER_VIDEO_SC7180))
		if (init_display_ops() != 0)
			printf("Failed to initialize display ops!\n");

	return 0;
}

INIT_FUNC(board_setup);
