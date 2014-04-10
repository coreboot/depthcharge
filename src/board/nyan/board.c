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
#include "board/nyan/power_ops.h"
#include "boot/fit.h"
#include "boot/ramoops.h"
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
#include "drivers/sound/i2s.h"
#include "drivers/sound/max98090.h"
#include "drivers/sound/tegra_ahub.h"
#include "drivers/storage/tegra_mmc.h"
#include "drivers/tpm/slb9635_i2c.h"
#include "drivers/tpm/tpm.h"
#include "vboot/util/flag.h"

static uint8_t board_id(void)
{
	static int id = -1;

	if (id < 0) {
		TegraGpio *q3 = new_tegra_gpio_input(GPIO(Q, 3));
		TegraGpio *t1 = new_tegra_gpio_input(GPIO(T, 1));
		TegraGpio *x1 = new_tegra_gpio_input(GPIO(X, 1));
		TegraGpio *x4 = new_tegra_gpio_input(GPIO(X, 4));

		id = q3->ops.get(&q3->ops) << 0 |
		     t1->ops.get(&t1->ops) << 1 |
		     x1->ops.get(&x1->ops) << 2 |
		     x4->ops.get(&x4->ops) << 3;
	}
	return id;
}

enum {
	BOARD_ID_REV0 = 0,
	BOARD_ID_REV1 = 9
};

enum {
	CLK_RST_BASE = 0x60006000,

	CLK_RST_L_RST_SET = CLK_RST_BASE + 0x300,
	CLK_RST_L_RST_CLR = CLK_RST_BASE + 0x304,
	CLK_RST_H_RST_SET = CLK_RST_BASE + 0x308,
	CLK_RST_H_RST_CLR = CLK_RST_BASE + 0x30c,
	CLK_RST_U_RST_SET = CLK_RST_BASE + 0x310,
	CLK_RST_U_RST_CLR = CLK_RST_BASE + 0x314
};

enum {
	CLK_L_I2C1 = 0x1 << 12,
	CLK_U_I2C3 = 0x1 << 3,
	CLK_H_I2C5 = 0x1 << 15
};

static int board_setup(void)
{
	uint8_t id = board_id();

	switch (id) {
	case BOARD_ID_REV0:
		fit_override_kernel_compat("google,nyan-rev0");
		break;
	case BOARD_ID_REV1:
		fit_override_kernel_compat("google,nyan-rev1");
		break;
	default:
		printf("Unrecognized board ID %#x.\n", id);
		return 1;
	}

	sysinfo_install_flags();

	TegraGpio *lid_switch = new_tegra_gpio_input(GPIO(R, 4));
	TegraGpio *ec_in_rw = new_tegra_gpio_input(GPIO(U, 4));
	flag_replace(FLAG_LIDSW, &lid_switch->ops);
	flag_install(FLAG_ECINRW, &ec_in_rw->ops);

	// The power switch is active low and needs to be inverted.
	TegraGpio *power_switch_l = new_tegra_gpio_input(GPIO(Q, 0));
	flag_replace(FLAG_PWRSW, new_gpio_not(&power_switch_l->ops));

	void *dma_channel_bases[32];
	for (int i = 0; i < ARRAY_SIZE(dma_channel_bases); i++)
		dma_channel_bases[i] = (void *)(0x60021000 + 0x40 * i);

	TegraApbDmaController *dma_controller =
		new_tegra_apb_dma((void *)0x60020000, dma_channel_bases,
				  ARRAY_SIZE(dma_channel_bases));

	TegraSpi *spi4 = new_tegra_spi(0x7000da00, dma_controller,
				       APBDMA_SLAVE_SL2B4);

	flash_set_ops(&new_spi_flash(&spi4->ops, 0x400000)->ops);

	TegraI2c *cam_i2c = new_tegra_i2c((void *)0x7000c500, 3,
					  (void *)CLK_RST_U_RST_SET,
					  (void *)CLK_RST_U_RST_CLR,
					  CLK_U_I2C3);

	tpm_set_ops(&new_slb9635_i2c(&cam_i2c->ops, 0x20)->base.ops);

	TegraSpi *spi1 = new_tegra_spi(0x7000d400, dma_controller,
				       APBDMA_SLAVE_SL2B1);

	TegraAudioHubXbar *xbar = new_tegra_audio_hub_xbar(0x70300800);
	TegraAudioHubApbif *apbif = new_tegra_audio_hub_apbif(0x70300000, 8);

	TegraI2s *i2s1 = new_tegra_i2s(0x70301100, &apbif->ops, 1, 16, 2,
				       1536000, 48000);
	TegraAudioHub *ahub = new_tegra_audio_hub(xbar, apbif, i2s1);
	I2sSource *i2s_source = new_i2s_source(&i2s1->ops, 48000, 2, 16000);
	SoundRoute *sound_route = new_sound_route(&i2s_source->ops);
	TegraI2c *i2c1 = new_tegra_i2c((void *)0x7000c000, 1,
				       (void *)CLK_RST_L_RST_SET,
				       (void *)CLK_RST_L_RST_CLR,
				       CLK_L_I2C1);
	Max98090Codec *codec = new_max98090_codec(
			&i2c1->ops, 0x10, 16, 48000, 256, 1);
	list_insert_after(&ahub->component.list_node, &sound_route->components);
	list_insert_after(&codec->component.list_node,
			  &sound_route->components);
	sound_set_ops(&sound_route->ops);

	cros_ec_set_bus(&new_cros_ec_spi_bus(&spi1->ops)->ops);

	// sdmmc4
	TegraMmcHost *emmc = new_tegra_mmc_host(0x700b0600, 8, 0, NULL);
	// sdmmc3
	TegraGpio *card_detect = new_tegra_gpio_input(GPIO(V, 2));
	GpioOps *card_detect_ops = &card_detect->ops;
	if (id != BOARD_ID_REV0)
		card_detect_ops = new_gpio_not(card_detect_ops);
	TegraMmcHost *sd_card = new_tegra_mmc_host(0x700b0400, 4, 1,
						   card_detect_ops);
	list_insert_after(&emmc->mmc.ctrlr.list_node,
			  &fixed_block_dev_controllers);
	list_insert_after(&sd_card->mmc.ctrlr.list_node,
			  &removable_block_dev_controllers);

	TegraI2c *pwr_i2c = new_tegra_i2c((void *)0x7000d000, 5,
					  (void *)CLK_RST_H_RST_SET,
					  (void *)CLK_RST_H_RST_CLR,
					  CLK_H_I2C5);
	As3722Pmic *pmic = new_as3722_pmic(&pwr_i2c->ops, 0x40);
	TegraGpio *reboot_gpio = new_tegra_gpio_output(GPIO(I, 5));
	NyanPowerOps *power = new_nyan_power_ops(&pmic->ops, &reboot_gpio->ops,
						 0);
	power_set_ops(&power->ops);

	/* Careful: the EHCI base is at offset 0x100 from the SoC's IP base */
	UsbHostController *usbd = new_usb_hc(EHCI, 0x7d000100);
	/* USB2 is connected to the camera, not needed in firmware */
	UsbHostController *usb3 = new_usb_hc(EHCI, 0x7d008100);

	list_insert_after(&usbd->list_node, &usb_host_controllers);
	list_insert_after(&usb3->list_node, &usb_host_controllers);

	ramoops_buffer(0x87f00000, 0x100000, 0x20000);

	return 0;
}

INIT_FUNC(board_setup);
