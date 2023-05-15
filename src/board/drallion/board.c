/*
 * Copyright 2019 Google LLC
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
#include <keycodes.h>
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
#include "drivers/storage/blockdev.h"
#include "drivers/storage/nvme.h"
#include "drivers/tpm/google/i2c.h"
#include "drivers/tpm/google/switches.h"
#include "drivers/tpm/tpm.h"
#include "vboot/util/flag.h"

#define POWER_BUTTON         0x90

enum {
	EC_HOST_BASE = 0x940,
	EC_PACKET_BASE = 0x950,
};

const char *pd_cbfs_firmware = "pdrw.bin";
const char *pd_cbfs_hash = "pdrw.hash";
//Ec Board Id to skip PD SW sync
uint32_t id_to_skip = 0;

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

static int keyboard_media_key_mapping(char ch)
{
	switch (ch) {
	case 0x57:
		return KEY_F(12);
	case 0x5e:
		return POWER_BUTTON;
	case 0x4b:
		return KEY_LEFT;
	case 0x4d:
		return KEY_RIGHT;
	case 0x48:
		return KEY_UP;
	case 0x50:
		return KEY_DOWN;
	default:
		return 0;
	}
}

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

	/* Update TI TCPC if available in CBFS and if the board is not id_to_skip*/
	if (cbfs_file_exists(pd_cbfs_hash) &&
	   (lib_sysinfo.board_id != id_to_skip)) {
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

	/* Initialize callback for media key mapping */
	initialize_keyboard_media_key_mapping_callback(keyboard_media_key_mapping);
	return 0;
}

INIT_FUNC(board_setup);
