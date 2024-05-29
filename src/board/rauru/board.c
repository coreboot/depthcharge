/* SPDX-License-Identifier: GPL-2.0 */

#include <assert.h>
#include <libpayload.h>

#include "base/init_funcs.h"
#include "drivers/bus/i2c/mtk_i2c.h"
#include "drivers/bus/spi/mtk.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/ec/cros/spi.h"
#include "drivers/flash/mtk_snfc.h"
#include "drivers/gpio/mtk_gpio.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/psci.h"
#include "drivers/sound/nau8318.h"
#include "drivers/storage/mtk_mmc.h"
#include "drivers/tpm/google/i2c.h"
#include "drivers/tpm/tpm.h"
#include "vboot/util/flag.h"

static int tpm_irq_status(void)
{
	static GpioOps *tpm_int;
	if (!tpm_int)
		tpm_int = sysinfo_lookup_gpio("TPM interrupt", 1,
					      new_mtk_eint);
	assert(tpm_int);
	return gpio_get(tpm_int);
}

static void enable_usb_vbus(struct UsbHostController *usb_host)
{
	/*
	 * To avoid USB detection issue, assert GPIO AP_XHCI_INIT_DONE
	 * to notify EC to enable USB VBUS when xHCI is initialized.
	 */
	GpioOps *pdn = sysinfo_lookup_gpio("XHCI init done", 1,
					   new_mtk_gpio_output);

	if (pdn) {
		gpio_set(pdn, 1);

		/* After USB VBUS is enabled, delay 500ms for USB detection. */
		mdelay(500);
	}
}

static void sound_setup(void)
{
	GpioOps *spk_en = sysinfo_lookup_gpio("speaker enable", 1,
					      new_mtk_gpio_output);
	GpioOps *beep_en = sysinfo_lookup_gpio("beep enable", 1,
					       new_mtk_gpio_output);
	if (!spk_en || !beep_en)
		return;

	nau8318Codec *nau8318 = new_nau8318_codec(spk_en, beep_en);
        sound_set_ops(&nau8318->ops);
}

static int board_setup(void)
{
	sysinfo_install_flags(new_mtk_gpio_input);
	power_set_ops(&psci_power_ops);

	/* Set up TPM */
	MTKI2c *i2c1 = new_mtk_i2c(0x13930000, 0x16380000, I2C_APDMA_ASYNC);
	GscI2c *tpm = new_gsc_i2c(&i2c1->ops, GSC_I2C_ADDR, &tpm_irq_status);
	tpm_set_ops(&tpm->base.ops);

	/* Set up EC */
	flag_replace(FLAG_LIDSW, cros_ec_lid_switch_flag());
	if (!CONFIG(DETACHABLE))
		flag_replace(FLAG_PWRSW, cros_ec_power_btn_flag());

	GpioOps *spi1_cs = new_gpio_not(new_mtk_gpio_output(PAD_SPI1_CSB));
	MtkSpi *spi1 = new_mtk_spi(0x16130000, spi1_cs);
	CrosEcSpiBus *cros_ec_spi_bus = new_cros_ec_spi_bus(&spi1->ops);
	GpioOps *ec_int = sysinfo_lookup_gpio("EC interrupt", 1,
					      new_mtk_gpio_input);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_spi_bus->ops, ec_int);
	register_vboot_ec(&cros_ec->vboot);

	sound_setup();

	/* Set up NOR flash ops */
	MtkNorFlash *nor_flash = new_mtk_nor_flash(0x16340000);
	flash_set_ops(&nor_flash->ops);

	/* Set up SD card */
	MtkMmcTuneReg sd_card_tune_reg = {
		.msdc_iocon = 0x0,
		.pad_tune = 0x0,
	};
	GpioOps *card_detect_ops = sysinfo_lookup_gpio("SD card detect", 1,
						       new_mtk_gpio_input);
	if (!card_detect_ops) {
		printf("%s: SD card detect GPIO not found\n", __func__);
	} else {
		MtkMmcHost *sd_card = new_mtk_mmc_host(0x16310000,
						       0x138a0000,
						       200 * MHz,
						       25 * MHz,
						       sd_card_tune_reg,
						       4, 1, card_detect_ops,
						       MTK_MMC_V2);

		list_insert_after(&sd_card->mmc.ctrlr.list_node,
				  &removable_block_dev_controllers);
	}

	/* Set up USB */
	UsbHostController *usb_host = new_usb_hc(XHCI, 0x16700000);
	set_usb_init_callback(usb_host, enable_usb_vbus);
	list_insert_after(&usb_host->list_node, &usb_host_controllers);

	return 0;
}

INIT_FUNC(board_setup);
