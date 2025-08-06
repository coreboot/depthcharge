/*
* This file is part of the coreboot project.
 *
 * Copyright (C) 2025, The Linux Foundation.  All rights reserved.
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
#include "vboot/util/flag.h"
#include "boot/fit.h"
#include "drivers/bus/i2c/qcom_qupv3_i2c.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/bus/spi/qcom_qspi.h"
#include "drivers/bus/spi/qcom_qupv3_spi.h"
#include "drivers/ec/cros/spi.h"
#include "drivers/flash/spi.h"
#include "drivers/gpio/gpio.h"
#include "drivers/gpio/qcom_gpio.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/psci.h"
#include "drivers/soc/x1p42100.h"
#include "drivers/storage/nvme.h"
#include "drivers/tpm/google/i2c.h"
#include <pci.h>
#include <pci/pci.h>
#include "variant.h"

static const VpdDeviceTreeMap vpd_dt_map[] = {
	{ "bluetooth_mac0", "bluetooth0/local-bd-address" },
	{ "wifi_mac0", "wifi0/local-mac-address" },
	{}
};

static void usb_setup(void)
{
	/* placeholder */
}

static void storage_setup(void)
{
	/* NVMe */
	NvmeCtrlr *nvme = new_nvme_ctrlr(variant_get_nvme_pcidev());
	list_insert_after(&nvme->ctrlr.list_node,
			  &fixed_block_dev_controllers);
}

static void flash_setup(void)
{
	/* SPI-NOR Flash driver - GPIO_132 as Chip Select */
	QcomQspi *spi_flash = new_qcom_qspi(QSPI_BASE, (GpioOps *)&new_gpio_output(QSPI_CS)->ops);
	SpiFlash *flash = new_spi_flash(&spi_flash->ops);
	flash_set_ops(&flash->ops);
}

static void ec_setup(void)
{
	SpiOps *ec_spi = &new_qup_spi(variant_get_ec_spi_base())->ops;
	CrosEcBusOps *ec_bus = &new_cros_ec_spi_bus(ec_spi)->ops;
	GpioOps *ec_int = sysinfo_lookup_gpio("EC interrupt", 1,
				new_gpio_input_from_coreboot);
	CrosEc *ec = new_cros_ec(ec_bus, ec_int);
	register_vboot_ec(&ec->vboot);
}

static int tpm_irq_status(void)
{
	static GpioOps *tpm_int = NULL;

	if (!tpm_int)
		tpm_int = sysinfo_lookup_gpio("TPM interrupt", 1,
					new_gpio_latched_from_coreboot);
	return gpio_get(tpm_int);
}

static void tpm_setup(void)
{
	tpm_set_ops(&new_gsc_i2c(&new_qup_i2c(variant_get_gsc_i2c_base())->ops, GSC_I2C_ADDR,
				  tpm_irq_status)->base.ops);
}

static void audio_setup(void)
{
	/* placeholder */
}

static void display_setup(void)
{
	/* placeholder */
}

static int board_setup(void)
{
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

	power_set_ops(&psci_power_ops);

	usb_setup();

	dt_register_vpd_mac_fixup(vpd_dt_map);

	storage_setup();

	flash_setup();

	if (CONFIG(DRIVER_EC_CROS))
		ec_setup();

	if (CONFIG(DRIVER_TPM_GOOGLE_TI50))
		tpm_setup();

	audio_setup();

	display_setup();

	return 0;
}

INIT_FUNC(board_setup);
