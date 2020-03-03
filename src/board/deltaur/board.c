// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2020 Google LLC
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
#include "drivers/gpio/tigerlake.h"
#include "drivers/gpio/gpio.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/pch.h"
#include "drivers/soc/tigerlake.h"
#include "drivers/sound/hda_codec.h"
#include "drivers/sound/sound.h"
#include "drivers/storage/ahci.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/nvme.h"
#include "drivers/tpm/cr50_i2c.h"
#include "drivers/tpm/cr50_switches.h"
#include "drivers/tpm/tpm.h"
#include "vboot/util/flag.h"

enum {
	EC_HOST_BASE = 0x940,
	EC_PACKET_BASE = 0x950,
};

static const char *pd_cbfs_firmware = "pdrw.bin";
static const char *pd_cbfs_hash = "pdrw.hash";

static const pcidev_t pch_spi_pci_dev = PCI_DEV(0, 0x1f, 5);
static const pcidev_t i2c4_pci_dev = PCI_DEV(0, 0x19, 0);
static const pcidev_t ahci_pci_dev = PCI_DEV(0, 0x17, 0);
static const pcidev_t pcie_root_port_9 = PCI_DEV(0, 0x1d, 0);

static const uint8_t cr50_addr = 0x50;

FlashProtectionMapping flash_protection_list[] = {
	/* TODO(b/150165222): fill out this list. */
	/* Empty entry for end of list */
	{{ 0 }}
};

static int cr50_irq_status(void)
{
	/* TODO(b/150165222): Not sure of the GPE number yet */
	return tigerlake_get_gpe(GPE0_DW0_15); /* GPP_C15 */
}

static int board_setup(void)
{
	sysinfo_install_flags(new_tigerlake_gpio_input_from_coreboot);

	/* 32MB SPI Flash */
	uintptr_t mmio_base = pci_read_config32(pch_spi_pci_dev,
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

	/* Update TI TCPC if available in CBFS.
	 */
	if (cbfs_find_file(pd_cbfs_hash, CBFS_TYPE_RAW)) {
		/*
		 * TODO(b/150165222): figure out if this is the same PD chip as
		 * previous generations, and make relevant changes if not.
		 */
		WilcoPd *ti_tcpc = new_wilco_pd_ti(wilco_ec, pd_cbfs_firmware,
						   pd_cbfs_hash);
		register_vboot_aux_fw(&ti_tcpc->ops);
	}

	/* H1 TPM on I2C bus 4 @ 400KHz, controller core is 133MHz */
	DesignwareI2c *i2c4 = new_pci_designware_i2c(
		i2c4_pci_dev, 400000, TIGERLAKE_DW_I2C_MHZ);

	Cr50I2c *tpm = new_cr50_i2c(&i2c4->ops, cr50_addr, &cr50_irq_status);
	tpm_set_ops(&tpm->base.ops);

	GpioOps *power_switch = &new_cr50_power_switch(&tpm->base.ops)->ops;
	flag_replace(FLAG_PWRSW, power_switch);
	flag_replace(FLAG_PHYS_PRESENCE, power_switch);

	/* PCH Power */
	power_set_ops(&tigerlake_power_ops);

	/* SATA AHCI */
	/* TODO(b/150165222): decide whether we need to keep this SATA config,
	 * only early-stage Sarien needed it but early-deltaur may too.
	 */
	AhciCtrlr *ahci = new_ahci_ctrlr(ahci_pci_dev);
	list_insert_after(&ahci->ctrlr.list_node, &fixed_block_dev_controllers);

	/* M.2 2280 SSD x4  */
	NvmeCtrlr *nvme_b = new_nvme_ctrlr(pcie_root_port_9);
	list_insert_after(&nvme_b->ctrlr.list_node,
			  &fixed_block_dev_controllers);

	/* Boot beep uses HDA codec */
	HdaCodec *codec = new_hda_codec();
	sound_set_ops(&codec->ops);
	set_hda_beep_nid_override(codec, 1);

	/*
	 * TODO(b/152383468): fill out mapping function after receiving the
	 * keyboard design.
	initialize_keyboard_media_key_mapping_callback(
		keyboard_media_key_mapping);
	*/
	return 0;
}

INIT_FUNC(board_setup);
