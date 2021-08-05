// SPDX-License-Identifier: GPL-2.0

/*
 * These needs to be included first.
 * Some of the driver headers would be dependent on these.
 */
#include <pci.h>
#include <pci/pci.h>

#include "base/init_funcs.h"
#include "base/list.h"
#include "board/brya/include/variant.h"
#include "drivers/bus/i2c/designware.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/flash/flash.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/gpio/alderlake.h"
#include "drivers/gpio/gpio.h"
#include "drivers/power/pch.h"
#include "drivers/soc/alderlake.h"
#include "drivers/tpm/cr50_i2c.h"
#include "drivers/tpm/tpm.h"
#include <libpayload.h>
#include <sysinfo.h>

#define TPM_I2C3	PCI_DEV(0, 0x15, 3)
#define TPM_I2C_ADDR	0x50
#define I2C_FS_HZ	400000

#define SSP_PORT_SPKR	2

__weak const struct storage_config *variant_get_storage_configs(size_t *count)
{
	static const struct storage_config storage_configs[] = {
		{ .media = STORAGE_NVME, .pci_dev = PCH_DEV_PCIE8 },
		{ .media = STORAGE_SDHCI, .pci_dev = PCH_DEV_PCIE7 },
	};

	*count = ARRAY_SIZE(storage_configs);
	return storage_configs;
}

static int cr50_irq_status(void)
{
	return alderlake_get_gpe(GPE0_DW0_13); /* GPP_A13 */
}

int board_get_ssp_port_index(void)
{
	return SSP_PORT_SPKR;
}

static void ec_setup(void)
{
	CrosEcLpcBus *cros_ec_lpc_bus =
		new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, NULL);
	register_vboot_ec(&cros_ec->vboot);
}

static void tpm_setup(void)
{
	DesignwareI2c *i2c3 = new_pci_designware_i2c(
		TPM_I2C3,
		I2C_FS_HZ, ALDERLAKE_DW_I2C_MHZ);
	tpm_set_ops(&new_cr50_i2c(&i2c3->ops, TPM_I2C_ADDR,
				  &cr50_irq_status)->base.ops);
}

static void audio_setup(void)
{
	brya_configure_audio(variant_probe_audio_config());
}

static void tcss_setup(void)
{
	size_t ports;
	const struct tcss_map *map = variant_get_tcss_map(&ports);
	register_tcss_ports(map, ports);
}

static int board_setup(void)
{
	sysinfo_install_flags(NULL);
	ec_setup();
	tpm_setup();
	power_set_ops(&alderlake_power_ops);
	audio_setup();
	tcss_setup();

	return 0;
}

INIT_FUNC(board_setup);
