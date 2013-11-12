/*
 * Copyright 2013 Google Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <libpayload.h>

#include "base/init_funcs.h"
#include "config.h"
#include "drivers/bus/i2c/tegra.h"
#include "drivers/bus/i2s/tegra.h"
#include "drivers/bus/spi/tegra.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/ec/cros/spi.h"
#include "drivers/dma/tegra_apb.h"
#include "drivers/flash/spi.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/gpio/tegra.h"
#include "drivers/power/as3722.h"
#include "drivers/sound/i2s.h"
#include "drivers/sound/max98090.h"
#include "drivers/sound/tegra_ahub.h"
#include "drivers/storage/tegra_mmc.h"
#include "drivers/tpm/slb9635_i2c.h"
#include "drivers/tpm/tpm.h"
#include "vboot/util/flag.h"

static int board_setup(void)
{
	if (sysinfo_install_flags())
		return 1;

	TegraGpio *lid_switch = new_tegra_gpio_input(GPIO_R, 4);
	TegraGpio *ec_in_rw = new_tegra_gpio_input(GPIO_U, 4);
	if (!lid_switch || !ec_in_rw ||
	    flag_replace(FLAG_LIDSW, &lid_switch->ops) ||
	    flag_install(FLAG_ECINRW, &ec_in_rw->ops))
		return 1;

	// The power switch is active low and needs to be inverted.
	TegraGpio *power_switch_l = new_tegra_gpio_input(GPIO_Q, 0);
	GpioOps *power_switch = new_gpio_not(&power_switch_l->ops);
	if (!power_switch_l || !power_switch ||
	    flag_replace(FLAG_PWRSW, power_switch))
		return 1;

	void *dma_channel_bases[32];
	for (int i = 0; i < ARRAY_SIZE(dma_channel_bases); i++)
		dma_channel_bases[i] = (void *)(0x60021000 + 0x40 * i);

	TegraApbDmaController *dma_controller =
		new_tegra_apb_dma((void *)0x60020000, dma_channel_bases,
				  ARRAY_SIZE(dma_channel_bases));
	if (!dma_controller)
		return 1;

	TegraSpi *spi4 = new_tegra_spi(0x7000da00, dma_controller,
				       APBDMA_SLAVE_SL2B4);
	if (!spi4)
		return 1;

	SpiFlash *flash = new_spi_flash(&spi4->ops, 0x400000);
	if (!flash || flash_set_ops(&flash->ops))
		return 1;

	TegraI2c *cam_i2c = new_tegra_i2c((void *)0x7000c500, 3);
	if (!cam_i2c)
		return 1;

	Slb9635I2c *tpm = new_slb9635_i2c(&cam_i2c->ops, 0x20);
	if (!tpm || tpm_set_ops(&tpm->base.ops))
		return 1;

	TegraSpi *spi1 = new_tegra_spi(0x7000d400, dma_controller,
				       APBDMA_SLAVE_SL2B1);
	if (!spi1)
		return 1;

	TegraAudioHubXbar *xbar = new_tegra_audio_hub_xbar(0x70300800);
	if (!xbar)
		return 1;
	TegraAudioHubApbif *apbif = new_tegra_audio_hub_apbif(0x70300000, 8);
	if (!apbif)
		return 1;

	TegraI2s *i2s1 = new_tegra_i2s(0x70301100, &apbif->ops, 1, 16, 2,
				       4800000, 48000);
	if (!i2s1)
		return 1;
	TegraAudioHub *ahub = new_tegra_audio_hub(xbar, apbif, i2s1);
	if (!ahub)
		return 1;
	I2sSource *i2s_source = new_i2s_source(&i2s1->ops, 48000, 2, 16000);
	if (!i2s_source)
		return 1;
	SoundRoute *sound_route = new_sound_route(&i2s_source->ops);
	if (!sound_route)
		return 1;
	TegraI2c *i2c1 = new_tegra_i2c((void *)0x7000c000, 1);
	Max98090Codec *codec = new_max98090_codec(
			&i2c1->ops, 0x10, 16, 48000, 256, 1);
	if (!codec)
		return 1;
	list_insert_after(&ahub->component.list_node, &sound_route->components);
	list_insert_after(&codec->component.list_node,
			  &sound_route->components);
	if (sound_set_ops(&sound_route->ops))
		return 1;

	CrosEcSpiBus *cros_ec_spi_bus = new_cros_ec_spi_bus(&spi1->ops);
	if (!cros_ec_spi_bus || cros_ec_set_bus(&cros_ec_spi_bus->ops))
		return 1;

	// sdmmc4
	TegraMmcHost *emmc = new_tegra_mmc_host(0x700b0600, 8, 0, NULL);
	// sdmmc3
	TegraGpio *card_detect = new_tegra_gpio_input(GPIO_V, 2);
	if (!card_detect)
		return 1;
	GpioOps *card_detect_ops = &card_detect->ops;
	if (!CONFIG_NYAN_IN_A_PIXEL) {
		card_detect_ops = new_gpio_not(card_detect_ops);
		if (!card_detect_ops)
			return 1;
	}
	TegraMmcHost *sd_card = new_tegra_mmc_host(0x700b0400, 4, 1,
						   card_detect_ops);
	if (!emmc || !sd_card)
		return 1;
	list_insert_after(&emmc->mmc.ctrlr.list_node,
			  &fixed_block_dev_controllers);
	list_insert_after(&sd_card->mmc.ctrlr.list_node,
			  &removable_block_dev_controllers);

	TegraI2c *pwr_i2c = new_tegra_i2c((void *)0x7000d000, 5);
	if (!pwr_i2c)
		return 1;
	As3722Pmic *pmic = new_as3722_pmic(&pwr_i2c->ops, 0x40);
	if (!pmic || power_set_ops(&pmic->ops))
		return 1;

	/* Careful: the EHCI base is at offset 0x100 from the SoC's IP base */
	UsbHostController *usbd = new_usb_hc(EHCI, 0x7d000100);
	/* USB2 is connected to the camera, not needed in firmware */
	UsbHostController *usb3 = new_usb_hc(EHCI, 0x7d008100);

	if (!usbd || !usb3)
		return 1;

	list_insert_after(&usbd->list_node, &usb_host_controllers);
	list_insert_after(&usb3->list_node, &usb_host_controllers);

	return 0;
}

INIT_FUNC(board_setup);
