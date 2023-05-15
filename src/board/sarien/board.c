/*
 * Copyright 2018 Google LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <arch/io.h>
#include <cbfs.h>
#include <libpayload.h>
#include <pci.h>
#include <pci/pci.h>
#include <sysinfo.h>

#include "base/init_funcs.h"
#include "base/list.h"
#include "drivers/bus/i2c/designware.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/ec/wilco/ec.h"
#include "drivers/ec/wilco/pd_ti.h"
#include "drivers/flash/fast_spi.h"
#include "drivers/flash/flash.h"
#include "drivers/gpio/cannonlake.h"
#include "drivers/gpio/gpio.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/pch.h"
#include "drivers/soc/cannonlake.h"
#include "drivers/sound/hda_codec.h"
#include "drivers/sound/sound.h"
#include "drivers/storage/ahci.h"
#include "drivers/storage/bayhub.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/nvme.h"
#include "drivers/storage/sdhci.h"
#include "drivers/tpm/google/i2c.h"
#include "drivers/tpm/google/switches.h"
#include "drivers/tpm/tpm.h"
#include "vboot/util/flag.h"

#define EMMC_SD_CLOCK_MIN       400000
#define EMMC_CLOCK_MAX          200000000

#define BH720_PCI_VID           0x1217
#define BH720_PCI_DID           0x8620

enum {
	EC_HOST_BASE = 0x940,
	EC_PACKET_BASE = 0x950,
};

const char *pd_cbfs_firmware = "pdrw.bin";
const char *pd_cbfs_hash = "pdrw.hash";

FlashProtectionMapping flash_protection_list[] = {
	{
		/* Macronix MX25L256 */
		.id = {
			.vendor = 0xc2,
			.model = 0x2019
		},
		.wp_status_value = 0x9c
	},
	{
		/* Winbond W25Q256 */
		.id = {
			.vendor = 0xef,
			.model = 0x7019
		},
		.wp_status_value = 0x9c
	},
	{
		/* GigaDevice GD25L256 */
		.id = {
			.vendor = 0xc8,
			.model = 0x4019
		},
		.wp_status_value = 0x9c
	},
	/* Empty entry for end of list */
	{{ 0 }}
};

static int gsc_irq_status(void)
{
	return cannonlake_get_gpe(GPE0_DW2_18);
}

static int board_setup(void)
{
	GscI2c *tpm;
	GpioOps *power_switch;

	sysinfo_install_flags(new_cannonlake_gpio_input_from_coreboot);

	/* 32MB SPI Flash */
	uintptr_t mmio_base = pci_read_config32(PCI_DEV(0, 0x1f, 5),
						PCI_BASE_ADDRESS_0);
	mmio_base &= PCI_BASE_ADDRESS_MEM_MASK;
	FastSpiFlash *spi = new_fast_spi_flash(mmio_base);
	flash_set_ops(&spi->ops);

	/* Wilco EC */
	WilcoEc *wilco_ec = new_wilco_ec(EC_HOST_BASE, EC_PACKET_BASE,
					 spi->region[FLASH_REGION_EC].offset,
					 spi->region[FLASH_REGION_EC].size);
	register_vboot_ec(&wilco_ec->vboot);
	flag_replace(FLAG_LIDSW, wilco_ec_lid_switch_flag(wilco_ec));

	/* Update TI TCPC if available in CBFS */
	if (cbfs_file_exists(pd_cbfs_hash)) {
		WilcoPd *ti_tcpc = new_wilco_pd_ti(wilco_ec, pd_cbfs_firmware,
						   pd_cbfs_hash);
		register_vboot_auxfw(&ti_tcpc->ops);
	}

	/* H1 TPM on I2C bus 4 @ 400KHz, controller core is 133MHz */
	DesignwareI2c *i2c4 = new_pci_designware_i2c(
		PCI_DEV(0, 0x19, 0), 400000, CANNONLAKE_DW_I2C_MHZ);

	tpm = new_gsc_i2c(&i2c4->ops, GSC_I2C_ADDR, &gsc_irq_status);
	tpm_set_ops(&tpm->base.ops);

	power_switch = &new_gsc_power_switch(&tpm->base.ops)->ops;
	flag_replace(FLAG_PWRSW, power_switch);
	flag_replace(FLAG_PHYS_PRESENCE, power_switch);

	/* Cannonlake PCH */
	power_set_ops(&cannonlake_power_ops);

	/* SATA AHCI */
	AhciCtrlr *ahci = new_ahci_ctrlr(PCI_DEV(0, 0x17, 0));
	list_insert_after(&ahci->ctrlr.list_node, &fixed_block_dev_controllers);

	SdhciHost *emmc = NULL;
	pcidev_t pci_dev;
	if (pci_find_device(BH720_PCI_VID, BH720_PCI_DID, &pci_dev)) {
		emmc = new_bayhub_sdhci_host(pci_dev, 0,
			EMMC_SD_CLOCK_MIN, EMMC_CLOCK_MAX);
		emmc->name = "eMMC";
		// TODO(b/168211204): Remove this quirk
		emmc->quirks |= SDHCI_QUIRK_CLEAR_TRANSFER_BEFORE_CMD;
	} else {
		printf("Failed to find BH720 with VID/DID %04x:%04x\n",
				BH720_PCI_VID, BH720_PCI_DID);
	}
	if (emmc) {
		list_insert_after(&emmc->mmc_ctrlr.ctrlr.list_node,
			&fixed_block_dev_controllers);
	}

	/* M.2 2280 SSD x4 */
	NvmeCtrlr *nvme = new_nvme_ctrlr(PCI_DEV(0, 0x1d, 4));
	list_insert_after(&nvme->ctrlr.list_node, &fixed_block_dev_controllers);

	/* M.2 2280 SSD x4 (if root ports are coalesced)  */
	NvmeCtrlr *nvme_b = new_nvme_ctrlr(PCI_DEV(0, 0x1d, 0));
	list_insert_after(&nvme_b->ctrlr.list_node,
			  &fixed_block_dev_controllers);

	/* Boot beep uses HDA codec */
	HdaCodec *codec = new_hda_codec();
	sound_set_ops(&codec->ops);
	set_hda_beep_nid_override(codec, 1);

	return 0;
}

INIT_FUNC(board_setup);
