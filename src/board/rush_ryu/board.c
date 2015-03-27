/*
 * Copyright 2014 Google Inc.
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

#include <assert.h>
#include <libpayload.h>

#include "base/init_funcs.h"
#include "boot/android_dt.h"
#include "boot/fit.h"
#include "boot/commandline.h"
#include "board/rush_ryu/fastboot.h"
#include "config.h"
#include "drivers/bus/spi/tegra.h"
#include "drivers/bus/i2c/tegra.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/gpio/gpio.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/gpio/tegra.h"
#include "drivers/dma/tegra_apb.h"
#include "drivers/flash/block_flash.h"
#include "drivers/flash/spi.h"
#include "drivers/power/sysinfo.h"
#include "drivers/power/tps65913.h"
#include "drivers/tpm/slb9635_i2c.h"
#include "drivers/tpm/tpm.h"
#include "drivers/storage/tegra_mmc.h"
#include "drivers/video/display.h"
#include "drivers/ec/cros/i2c.h"
#include "vboot/boot_policy.h"
#include "vboot/util/flag.h"
#include "drivers/video/tegra132.h"
#include "drivers/sound/i2s.h"
#include "drivers/sound/rt5677.h"
#include "drivers/sound/tegra_ahub.h"

enum {
	CLK_RST_BASE = 0x60006000,

	CLK_RST_L_RST_SET = CLK_RST_BASE + 0x300,
	CLK_RST_L_RST_CLR = CLK_RST_BASE + 0x304,
	CLK_RST_H_RST_SET = CLK_RST_BASE + 0x308,
	CLK_RST_H_RST_CLR = CLK_RST_BASE + 0x30c,
	CLK_RST_U_RST_SET = CLK_RST_BASE + 0x310,
	CLK_RST_U_RST_CLR = CLK_RST_BASE + 0x314,
	CLK_RST_X_RST_SET = CLK_RST_BASE + 0x290,
	CLK_RST_X_RST_CLR = CLK_RST_BASE + 0x294
};

enum {
	CLK_L_I2C1 = 0x1 << 12,
	CLK_H_I2C2 = 0x1 << 22,
	CLK_U_I2C3 = 0x1 << 3,
	CLK_H_I2C5 = 0x1 << 15,
	CLK_X_I2C6 = 0x1 << 6
};

const char *mainboard_commandline(void)
{
	return NULL;
}

const char *hardware_name(void)
{
	return "dragon";
}

static void choose_devicetree_by_boardid(void)
{
	fit_set_compat_by_rev("google,ryu-rev%d", lib_sysinfo.board_id);
}

static int board_setup(void)
{
	sysinfo_install_flags(new_tegra_gpio_input_from_coreboot);

	choose_devicetree_by_boardid();

	static const struct boot_policy policy[] = {
		{KERNEL_IMAGE_BOOTIMG, CMD_LINE_DTB},
		{KERNEL_IMAGE_CROS, CMD_LINE_SIGNER},
	};

	if (set_boot_policy(policy, ARRAY_SIZE(policy)) == -1)
		halt();

	void *dma_channel_bases[32];
	for (int i = 0; i < ARRAY_SIZE(dma_channel_bases); i++)
		dma_channel_bases[i] = (void *)((unsigned long)0x60021000 + 0x40 * i);

	TegraApbDmaController *dma_controller =
		new_tegra_apb_dma((void *)0x60020000, dma_channel_bases,
				  ARRAY_SIZE(dma_channel_bases));

	TegraSpi *spi4 = new_tegra_spi(0x7000da00, dma_controller,
				       APBDMA_SLAVE_SL2B4);

	SpiFlash *flash = new_spi_flash(&spi4->ops);

	flash_set_ops(&flash->ops);

	FlashBlockDev *fbdev = block_flash_register_nor(&flash->ops);

	TegraI2c *cam_i2c = new_tegra_i2c((void *)0x7000c500, 3,
					  (void *)CLK_RST_U_RST_SET,
					  (void *)CLK_RST_U_RST_CLR,
					  CLK_U_I2C3);

	tpm_set_ops(&new_slb9635_i2c(&cam_i2c->ops, 0x20)->base.ops);

	TegraI2c *ec_i2c = new_tegra_i2c((void *)0x7000c400, 2,
					  (void *)CLK_RST_H_RST_SET,
					  (void *)CLK_RST_H_RST_CLR,
					  CLK_H_I2C2);

	CrosEcI2cBus *cros_ec_i2c_bus = new_cros_ec_i2c_bus(&ec_i2c->ops, 0x1E);
	cros_ec_set_bus(&cros_ec_i2c_bus->ops);

	TegraI2c *pwr_i2c = new_tegra_i2c((void *)0x7000d000, 5,
					  (void *)CLK_RST_H_RST_SET,
					  (void *)CLK_RST_H_RST_CLR,
					  CLK_H_I2C5);

	Tps65913Pmic *pmic = new_tps65913_pmic(&pwr_i2c->ops, 0x58);
	SysinfoResetPowerOps *power = new_sysinfo_reset_power_ops(&pmic->ops,
			new_tegra_gpio_output_from_coreboot);
	power_set_ops(&power->ops);

	/* sdmmc4 */
	TegraMmcHost *emmc = new_tegra_mmc_host(0x700b0600, 8, 0, NULL, NULL);

	list_insert_after(&emmc->mmc.ctrlr.list_node,
			  &fixed_block_dev_controllers);

	/* Fill in fastboot related information */
	BlockDevCtrlr *bdev_arr[BDEV_COUNT] = {
		[FLASH_BDEV] = &fbdev->ctrlr,
		[MMC_BDEV] = &emmc->mmc.ctrlr,
	};
	fill_fb_info(bdev_arr);

	/* Careful: the EHCI base is at offset 0x100 from the SoC's IP base */
	UsbHostController *usbd = new_usb_hc(EHCI, 0x7d000100);

	list_insert_after(&usbd->list_node, &usb_host_controllers);

	/* Lid always open for now. */
	flag_replace(FLAG_LIDSW, new_gpio_high());

	/* Audio init */
	TegraAudioHubXbar *xbar = new_tegra_audio_hub_xbar(0x70300800);
	TegraAudioHubApbif *apbif = new_tegra_audio_hub_apbif(0x70300000, 8);
	TegraI2s *i2s1 = new_tegra_i2s(0x70301100, &apbif->ops, 1, 16, 2,
				       1536000, 48000);
	TegraAudioHub *ahub = new_tegra_audio_hub(xbar, apbif, i2s1);
	I2sSource *i2s_source = new_i2s_source(&i2s1->ops, 48000, 2, 16000);
	SoundRoute *sound_route = new_sound_route(&i2s_source->ops);
	TegraI2c *i2c6 = new_tegra_i2c((void *)0x7000d100, 6,
				       (void *)CLK_RST_X_RST_SET,
				       (void *)CLK_RST_X_RST_CLR,
				       CLK_X_I2C6);
	rt5677Codec *codec = new_rt5677_codec(&i2c6->ops, 0x2D, 16, 48000, 256, 1);	//0x2C for P0
	list_insert_after(&ahub->component.list_node, &sound_route->components);
	list_insert_after(&codec->component.list_node, &sound_route->components);

	sound_set_ops(&sound_route->ops);

	return 0;
}

INIT_FUNC(board_setup);

static TegraI2c *get_backlight_i2c(void)
{
	static TegraI2c *backlight_i2c;

	if (backlight_i2c == NULL)
		backlight_i2c = new_tegra_i2c((void *)0x7000d100, 6,
					(void *)CLK_RST_X_RST_SET,
					(void *)CLK_RST_X_RST_CLR,
					CLK_X_I2C6);
	return backlight_i2c;
}

/* Turn on or turn off the backlight */
static int ryu_backlight_update(DisplayOps *me, uint8_t enable)
{
	struct bl_reg {
		uint8_t reg;
		uint8_t val;
	};

	static const struct bl_reg bl_on_list[] = {
		{0x10, 0x01},	/* Brightness mode: BRTHI/BRTLO */
		{0x11, 0x05},	/* maxcurrent: 20ma */
		{0x14, 0x7f},	/* ov: 2v, all 6 current sinks enabled */
		{0x00, 0x01},	/* backlight on */
		{0x04, 0x55},	/* brightness: BRT[11:4] */
				/*             0x000: 0%, 0xFFF: 100% */
	};

	static const struct bl_reg bl_off_list[] = {
		{0x00, 0x00},	/* backlight off */
	};

	TegraI2c *backlight_i2c = get_backlight_i2c();
	const struct bl_reg *current;
	size_t size, i;

	if (enable) {
		current = bl_on_list;
		size = ARRAY_SIZE(bl_on_list);
	} else {
		current = bl_off_list;
		size = ARRAY_SIZE(bl_off_list);
	}

	for (i = 0; i < size; ++i) {
		i2c_writeb(&backlight_i2c->ops, 0x2c, current->reg,
				current->val);
		++current;
	}

	return 0;
}

static DisplayOps ryu_display_ops = {
	.init = &tegra132_display_init,
	.backlight_update = &ryu_backlight_update,
	.stop = &tegra132_display_stop,
};

static int display_setup(void)
{
	if (lib_sysinfo.framebuffer == NULL ||
		lib_sysinfo.framebuffer->physical_address == 0)
		return 0;

	display_set_ops(&ryu_display_ops);

	return 0;
}

INIT_FUNC(display_setup);
