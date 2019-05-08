/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2018, The Linux Foundation.  All rights reserved.
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
#include <stdio.h>
#include <stdint.h>

#include "base/init_funcs.h"
#include "base/dt_set_wifi_calibration.h"
#include "drivers/gpio/gpio.h"
#include "vboot/util/flag.h"
#include "boot/fit.h"
#include "drivers/bus/spi/qcs405.h"
#include "drivers/bus/i2c/qcs405.h"
#include "drivers/bus/i2c/qcs405_blsp.h"
#include "drivers/video/led_lp5562.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/power/psci.h"
#include "drivers/storage/sdhci_msm.h"
#include "drivers/gpio/sysinfo.h"
#include "board.h"
#include "drivers/tpm/cr50_switches.h"
#include "drivers/tpm/spi.h"
#include "drivers/tpm/tpm.h"

#define TLMM_BOOT_SEL		0x010C1000
#define EMMC_BOOT		0x8000000
#define SDC1_HC_BASE		0x07804000
#define SDC1_TLMM_CFG_ADDR	0x10C2000


static const VpdDeviceTreeMap vpd_dt_map[] = {
	{ "cherokee_mac", "/soc@0/wifi@a000000/local-mac-address" },
	{ "eth_wan_mac", "/soc@0/ethernet@7a80000/mdio/switch0@16/ports/port@2/local-mac-address" },
	{ "eth_lan_mac", "/soc@0/ethernet@7a80000/mdio/switch0@16/ports/port@3/local-mac-address" },
	{ "bluetooth_mac", "/soc@0/serial@78b2000/wcn3990-bt/local-bd-address" },
	{ "cascade_mac", "/soc@0/pci@10000000/pcie@0/wifi@0,0/local-mac-address" },

	{}
};

static const DtPathMap calibration_maps[] = {
	{1, "/soc@0/pci@10000000/pcie@0/wifi@0,0/qcom,ath10k-pre-calibration-data",
	"wifi_base64_calibration0"},
	{}
};

static const DtPathMap cc_maps[] = {
	{1, "/soc@0/pci@10000000/pcie@0/wifi@0,0", "qcom,ath10k-country-code"},
	{1, "/soc@0/wifi@a000000", "qcom,ath10k-country-code"},
	{}
};

static const DtPathMap xo_cal_map[] = {
	{1, "/soc@0/wifi@a000000", "xo-cal-data" },
	{}
};

static const DtPathMap bt_fw_map[] = {
	{1, "/soc@0/serial@78b2000/wcn3990-bt", "firmware-name" },
	{1, "/soc@0/serial@78b2000/wcn3998-bt", "firmware-name" },
	{}
};

/* If CC not present in VPD, use first entry.
 * If CC not found in map, use last entry.
 */
static const CcFwMap cc_fw_map[] ={
	{"us", "crnv30.bin"},
	{"ca", "crnv30.bin"},
	{"e-", "crnv30_eu.bin"},
	{}
};

static void runtime_dt_select(void)
{
	printf("\nBOARD ID: %d\n", lib_sysinfo.board_id);
	switch(lib_sysinfo.board_id) {
		case BOARD_ID_MISTRAL_EVB:
			fit_add_compat("qcom,qcs404-evb");
			fit_add_compat("google,mistral-buck");
			break;
		case BOARD_ID_MISTRAL_PROTO_INT_BUCK:
			fit_add_compat("google,mistral");
			fit_add_compat("google,mistral-buck");
			break;
		case BOARD_ID_MISTRAL_PROTO_EXT_BUCK:
		default:
			fit_add_compat("google,mistral-buck");
			fit_add_compat("google,mistral");
			fit_add_compat("qcom,qcs404-evb");
			break;
	}
}

static int fix_device_tree(DeviceTreeFixup *fixup, DeviceTree *tree)
{
	int rv = 0;

	rv = dt_set_wifi_calibration(tree, calibration_maps);
	rv |= dt_set_wifi_country_code(tree, cc_maps);
	rv |= dt_set_xo_cal_data(tree, xo_cal_map);
	rv |= dt_set_bt_fw_name(tree, bt_fw_map, cc_fw_map);

	return rv;
}

static DeviceTreeFixup ipq_enet_fixup = {
	.fixup = fix_device_tree
};

static int board_setup(void)
{
	sysinfo_install_flags(NULL);

	power_set_ops(&psci_power_ops);

	UsbHostController *usb_host2 = new_usb_hc(XHCI, 0x78C0000);

	list_insert_after(&usb_host2->list_node, &usb_host_controllers);
	fit_add_compat("qcom,qcs404-evb");

	/* Add device-tree compatible string
	 * based on Board-id.
	 */
	runtime_dt_select();

	dt_register_vpd_mac_fixup(vpd_dt_map);

	list_insert_after(&ipq_enet_fixup.list_node, &device_tree_fixups);

#ifdef CONFIG_DRIVER_BUS_SPI_QCS405
	SpiController *spi_flash = new_spi(BLSP5_SPI, 0);
	SpiFlash *flash = new_spi_flash(&spi_flash->ops);
	flash_set_ops(&flash->ops);

	SpiController *spi_tpm = new_spi(4, 0);
	SpiTpm *tpm = new_tpm_spi(&spi_tpm->ops, NULL);
	tpm_set_ops(&tpm->ops);
	flag_replace(FLAG_PHYS_PRESENCE, &new_cr50_rec_switch(&tpm->ops)->ops);
#endif

	/* eMMC support */
	/* DATA pins are muxed between NAND and EMMC, configure them to eMMC */
	writel(EMMC_BOOT, (uint32_t *)TLMM_BOOT_SEL);

	SdhciHost *emmc = new_sdhci_msm_host((void *)SDC1_HC_BASE,
					SDHCI_PLATFORM_EMMC_1V8_POWER |
					SDHCI_PLATFORM_NO_EMMC_HS200,
					100*MHz,
					(void *)SDC1_TLMM_CFG_ADDR,
					NULL);
	assert(emmc);
	list_insert_after(&emmc->mmc_ctrlr.ctrlr.list_node,
			&fixed_block_dev_controllers);

	DisplayOps *led_ops = new_led_lp5562_display
		(&new_qcs405_i2c(BLSP_QUP_ID_1)->ops, 0x30);
	display_set_ops(led_ops);

	display_init();

	return 0;
}

INIT_FUNC(board_setup);
