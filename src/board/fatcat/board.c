// SPDX-License-Identifier: GPL-2.0

/*
 * These needs to be included first.
 * Some of the driver headers would be dependent on these.
 */
#include <commonlib/list.h>
#include <pci.h>
#include <pci/pci.h>

#include "base/init_funcs.h"
#include "board/fatcat/include/variant.h"
#include "drivers/bus/i2c/designware.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/flash/fast_spi.h"
#include "drivers/flash/flash.h"
#include "drivers/flash/memmapped.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/gpio/gpio.h"
#include "drivers/gpio/pantherlake.h"
#include "drivers/power/pch.h"
#include "drivers/soc/pantherlake.h"
#include "drivers/storage/storage_common.h"
#include "drivers/tpm/google/i2c.h"
#include "drivers/tpm/google/switches.h"
#include "drivers/tpm/tpm.h"
#include "vboot/util/flag.h"
#include <libpayload.h>
#include <sysinfo.h>

#if CONFIG(CROS_EC_ENABLE_MEC)
#define EC_SOC_INT_ODL	0 /* Not Connected */
#else
#define EC_SOC_INT_ODL	GPP_E07
#endif

#define I2C_FS_HZ	400000 /* 400KHz */

__weak const struct storage_config *variant_get_storage_configs(size_t *count)
{
	static const struct storage_config storage_configs[] = {
		{ .media = STORAGE_NVME, .pci_dev = PCI_DEV_PCIE9 },
	};

	*count = ARRAY_SIZE(storage_configs);
	return storage_configs;
}

static bool is_board_fatcat(void)
{
	static const char * const board_str = "fatcat";

	return strncmp(CONFIG_VARIANT_NAME, board_str, strlen(board_str)) == 0;
}

__weak const int variant_get_ec_int(void)
{
	return EC_SOC_INT_ODL;
}

static GpioOps *mkbp_int_ops(void)
{
	GpioCfg *mkbp_int_gpio = new_platform_gpio_input(variant_get_ec_int());
	if (is_board_fatcat())
		return &mkbp_int_gpio->ops;
	else
		return new_gpio_not(&mkbp_int_gpio->ops); // Active-low, has to be inverted
}

static void ec_setup(void)
{
	/* Chrome EC (eSPI) */
	if (!CONFIG(DRIVER_EC_CROS_LPC))
		return;

	CrosEcLpcBus *cros_ec_lpc_bus;
	CrosEcLpcBusVariant variant = CROS_EC_LPC_BUS_GENERIC;

	if (CONFIG(CROS_EC_ENABLE_MEC))
		variant = CROS_EC_LPC_BUS_MEC;

	cros_ec_lpc_bus = new_cros_ec_lpc_bus(variant);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, mkbp_int_ops());
	register_vboot_ec(&cros_ec->vboot);
}

__weak const struct tpm_config *variant_get_tpm_config(void)
{
	static const struct tpm_config config = {
		.pci_dev = PCI_DEV_I2C3,
	};

	return &config;
}

__weak int gsc_irq_status(void)
{
	return pantherlake_get_gpe(GPE0_DW1_15); /* GPP_D15 */
}

static void tpm_setup(void)
{
	const struct tpm_config *config = variant_get_tpm_config();
	DesignwareI2c *i2c = new_pci_designware_i2c(config->pci_dev,
						    I2C_FS_HZ,
						    PANTHERLAKE_DW_I2C_MHZ);
	GscI2c *tpm = new_gsc_i2c(&i2c->ops, GSC_I2C_ADDR,
				    &gsc_irq_status);
	tpm_set_ops(&tpm->base.ops);
	if (CONFIG(TPM_GOOGLE_SWITCHES))
		flag_replace(FLAG_PHYS_PRESENCE,
				&new_gsc_rec_switch(&tpm->base.ops)->ops);
}

__weak const struct audio_config *variant_probe_audio_config(void)
{
	static struct audio_config config;

	printf("%s Empty stub. Please add variant audio config\n", __func__);
	return &config;
}

static void flash_setup(void)
{
	if(CONFIG(DRIVER_FLASH_FAST_SPI)) {
		/* 32MB SPI Flash */
		uintptr_t mmio_base = pci_read_config32(PCI_DEV_SPI,
							PCI_BASE_ADDRESS_0);
		mmio_base &= PCI_BASE_ADDRESS_MEM_MASK;
		FastSpiFlash *spi = new_fast_spi_flash(mmio_base);
		MmapFlash *flash = new_mmap_backed_flash(&spi->ops);
		flash_set_ops(&flash->ops);
	} else {
		flash_set_ops(&new_mmap_flash()->ops);
	}
}

static int board_setup(void)
{
	sysinfo_install_flags(NULL);
	ec_setup();
	tpm_setup();
	power_set_ops(&pantherlake_power_ops);
	flash_setup();
	common_audio_setup(variant_probe_audio_config());

	return 0;
}

INIT_FUNC(board_setup);
