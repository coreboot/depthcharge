/*
 * Copyright 2015 Google Inc.
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
#include <vboot_api.h>
#include <vboot_nvstorage.h>

#include "base/init_funcs.h"
#include "base/cleanup_funcs.h"
#include "boot/android_dt.h"
#include "boot/bcb.h"
#include "boot/fit.h"
#include "boot/commandline.h"
#include "board/smaug/fastboot.h"
#include "config.h"
#include "drivers/bus/spi/tegra.h"
#include "drivers/bus/i2c/tegra.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/gpio/gpio.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/gpio/tegra210.h"
#include "drivers/dma/tegra_apb.h"
#include "drivers/flash/block_flash.h"
#include "drivers/flash/spi.h"
#include "drivers/power/sysinfo.h"
#include "drivers/power/max77620.h"
#include "drivers/tpm/slb9635_i2c.h"
#include "drivers/tpm/tpm.h"
#include "drivers/storage/tegra_mmc.h"
#include "drivers/video/display.h"
#include "drivers/ec/cros/i2c.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/video/coreboot_fb.h"
#include "vboot/boot_policy.h"
#include "vboot/stages.h"
#include "vboot/util/flag.h"
#include "vboot/vbnv.h"
#include "drivers/video/tegra132.h"
#include "drivers/sound/i2s.h"
#include "drivers/sound/rt5677.h"
#include "drivers/sound/tegra_ahub.h"

#define AXBAR_BASE		0x702D0800
#define ADMAIF_BASE		0x702D0000
#define I2S1_BASE		0x702D1000
#define I2C6_BASE		0x7000D100
#define RT5677_DEV_NUM		0x2D

enum {
	CLK_RST_BASE = 0x60006000,

	CLK_RST_L_RST_SET = CLK_RST_BASE + 0x300,
	CLK_RST_L_RST_CLR = CLK_RST_BASE + 0x304,
	CLK_RST_H_RST_SET = CLK_RST_BASE + 0x308,
	CLK_RST_H_RST_CLR = CLK_RST_BASE + 0x30c,
	CLK_RST_U_RST_SET = CLK_RST_BASE + 0x310,
	CLK_RST_U_RST_CLR = CLK_RST_BASE + 0x314,
	CLK_RST_X_RST_SET = CLK_RST_BASE + 0x290,
	CLK_RST_X_RST_CLR = CLK_RST_BASE + 0x294,
	CLK_RST_Y_RST_SET = CLK_RST_BASE + 0x2a8,
	CLK_RST_Y_RST_CLR = CLK_RST_BASE + 0x2ac,
};

enum {
	CLK_L_I2C1 = 0x1 << 12,
	CLK_H_I2C2 = 0x1 << 22,
	CLK_U_I2C3 = 0x1 << 3,
	CLK_H_I2C5 = 0x1 << 15,
	CLK_X_I2C6 = 0x1 << 6
};

#define BOOTREASON_SCRATCH_REG		((void *)(uintptr_t)(0x7000EC48ULL))

enum {
	PMC_BOOTREASON_MASK = 0x7,
	PMC_BOOTREASON_REBOOT = 0x1,
	PMC_BOOTREASON_PANIC = 0x2,
	PMC_BOOTREASON_WATCHDOG = 0x3,
	PMC_BOOTREASON_SENSOR = 0x4,
};

static const char * const bootreason_string[] = {
	[PMC_BOOTREASON_REBOOT] = "reboot",
	[PMC_BOOTREASON_PANIC] = "kernel_panic",
	[PMC_BOOTREASON_WATCHDOG] = "watchdog",
	[PMC_BOOTREASON_SENSOR] = "oemerr_thermal",
};

enum {
	MC_BASE = 0x70019000,
	VIDEO_PROTECT_BOM = MC_BASE + 0x648,
	VIDEO_PROTECT_SIZE_MB = MC_BASE + 0x64C,
};

const char *get_bootreason(void)
{
	uint32_t val = read32(BOOTREASON_SCRATCH_REG) & PMC_BOOTREASON_MASK;

	if ((val < PMC_BOOTREASON_REBOOT) || (val > PMC_BOOTREASON_SENSOR))
		val = PMC_BOOTREASON_REBOOT;

	return bootreason_string[val];
}

const char *mainboard_commandline(void)
{
	static char vpr[80];
	uint32_t vsize, vaddr;

	/* Get Video Protect Region (VPR) size/address for kernel use */
	vsize = readl((void *)VIDEO_PROTECT_SIZE_MB);
	vaddr = readl((void *)VIDEO_PROTECT_BOM);
	snprintf(vpr, sizeof(vpr), "vpr=0x%08x@0x%08x ", vsize << 20, vaddr);

	return vpr;
}

const char *hardware_name(void)
{
	return "dragon";
}

static void choose_devicetree_by_boardid(void)
{
	fit_set_compat_by_rev("google,smaug-rev%d", lib_sysinfo.board_id);
}

static TegraI2c *get_i2c6(void)
{
	static TegraI2c *i2c6;

	if (i2c6 == NULL)
		i2c6 = new_tegra_i2c((void *)I2C6_BASE, 6,
					(void *)CLK_RST_X_RST_SET,
					(void *)CLK_RST_X_RST_CLR,
					CLK_X_I2C6);
	return i2c6;
}

static BlockDevCtrlr *bcb_bdev_ctrlr;

BlockDevCtrlr *bcb_board_bdev_ctrlr(void)
{
	return bcb_bdev_ctrlr;
}

#define SPI_FLASH_BLOCK_ERASE_64KB	0xd8

/*
 * Override SPI flash params sector_size and erase_cmd passed in by
 * coreboot. Smaug uses Winbond W25Q128FW which takes 3x time to erase 64KiB
 * using sector erase (4KiB) as compared to block erase (64KiB).
 *
 * Coreboot needs to use sector erase (4KiB) to ensure that vbnv_erase does not
 * clear off unwanted parts of the flash.
 *
 * Thus, override sector_size to be equal to 64KiB and erase command as block
 * erase here so that fastboot can operate on BLOCK_SIZE and achieve faster
 * write time.
 */
static void flash_params_override(void)
{
	lib_sysinfo.spi_flash.sector_size = 64 * KiB;
	lib_sysinfo.spi_flash.erase_cmd = SPI_FLASH_BLOCK_ERASE_64KB;
}

static void update_boot_on_ac_detect_state(void)
{
	uint32_t boot_on_ac = vbnv_read(VBNV_BOOT_ON_AC_DETECT);

	if (cros_ec_set_boot_on_ac(boot_on_ac) != 0)
		printf("ERROR: Could not set boot_on_ac\n");
}

static int smaug_display_lines_cleanup(struct CleanupFunc *cleanup,
				       CleanupType type)
{
	if ((type != CleanupOnReboot) && (type != CleanupOnPowerOff))
		return 0;

	/* LCD_RST_L (V2) - set to 0. */
	TegraGpio *lcd_rst = new_tegra_gpio_output(GPIO(V, 2));
	lcd_rst->ops.set(&lcd_rst->ops, 0);
	mdelay(30);

	/* LCD_EN (V1) - set to 0. */
	TegraGpio *lcd_en = new_tegra_gpio_output(GPIO(V, 1));
	lcd_en->ops.set(&lcd_en->ops, 0);
	mdelay(2);

	/* EN_VDD18_LCD (V3) - set to 0. */
	TegraGpio *vdd18_lcd = new_tegra_gpio_output(GPIO(V, 3));
	vdd18_lcd->ops.set(&vdd18_lcd->ops, 0);
	mdelay(6);

	/* EN_VDD_LCD (V4) - set to 0. */
	TegraGpio *vdd_lcd = new_tegra_gpio_output(GPIO(V, 4));
	vdd_lcd->ops.set(&vdd_lcd->ops, 0);

	return 0;
}

static int smaug_touch_deassert(struct CleanupFunc *cleanup,
				CleanupType type)
{
	if (type != CleanupOnHandoff)
		return 0;

	/* TOUCH_RST_L(V6) -- Set to 1 */
	TegraGpio *touch_rst = new_tegra_gpio_output(GPIO(V, 6));
	touch_rst->ops.set(&touch_rst->ops, 1);

	return 0;
}

static int board_setup(void)
{
	flash_params_override();

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
		dma_channel_bases[i] = (void *)((unsigned long)0x60021000
						+ 0x40 * i);

	TegraApbDmaController *dma_controller =
		new_tegra_apb_dma((void *)0x60020000, dma_channel_bases,
				  ARRAY_SIZE(dma_channel_bases));

	TegraSpi *qspi = new_tegra_spi(0x70410000, dma_controller,
				       APBDMA_SLAVE_QSPI);

	SpiFlash *flash = new_spi_flash(&qspi->ops);

	flash_set_ops(&flash->ops);

	FlashBlockDev *fbdev = block_flash_register_nor(&flash->ops);

	TegraI2c *gen3_i2c = new_tegra_i2c((void *)0x7000c500, 3,
					   (void *)CLK_RST_U_RST_SET,
					   (void *)CLK_RST_U_RST_CLR,
					   CLK_U_I2C3);

	tpm_set_ops(&new_slb9635_i2c(&gen3_i2c->ops, 0x20)->base.ops);

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

	Max77620Pmic *pmic = new_max77620_pmic(&pwr_i2c->ops, 0x3c);
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

	/* Bdev ctrlr required for BCB. */
	bcb_bdev_ctrlr = &emmc->mmc.ctrlr;

	/* Careful: the EHCI base is at offset 0x100 from the SoC's IP base */
	UsbHostController *usbd = new_usb_hc(EHCI, 0x7d000100);

	list_insert_after(&usbd->list_node, &usb_host_controllers);

	/* Lid always open for now. */
	flag_replace(FLAG_LIDSW, new_gpio_high());

	/* Audio init */
	TegraAudioHubXbar *xbar = new_tegra_audio_hub_xbar(AXBAR_BASE);
	TegraAudioHubApbif *apbif = new_tegra_audio_hub_apbif(ADMAIF_BASE, 8);
	TegraI2s *i2s1 = new_tegra_i2s(I2S1_BASE, &apbif->ops, 1, 16, 2,
				       1536000, 48000);
	TegraAudioHub *ahub = new_tegra_audio_hub(xbar, apbif, i2s1);
	I2sSource *i2s_source = new_i2s_source(&i2s1->ops, 48000, 2, 16000);
	SoundRoute *sound_route = new_sound_route(&i2s_source->ops);
	TegraI2c *i2c6 = get_i2c6();
	rt5677Codec *codec = new_rt5677_codec(&i2c6->ops, RT5677_DEV_NUM, 16, 48000, 256, 1);
	list_insert_after(&ahub->component.list_node, &sound_route->components);
	list_insert_after(&codec->component.list_node, &sound_route->components);

	sound_set_ops(&sound_route->ops);

	/* Update BOOT_ON_AC_DETECT state */
	update_boot_on_ac_detect_state();

	static CleanupFunc disp = {
		&smaug_display_lines_cleanup,
		CleanupOnPowerOff | CleanupOnReboot,
		NULL
	};

	list_insert_after(&disp.list_node, &cleanup_funcs);

	static CleanupFunc touch = {
		&smaug_touch_deassert,
		CleanupOnHandoff,
		NULL
	};

	list_insert_after(&touch.list_node, &cleanup_funcs);

	return 0;
}

INIT_FUNC(board_setup);

/* Turn on or turn off the backlight */
static int smaug_backlight_update(DisplayOps *me, uint8_t enable)
{
	struct bl_reg {
		uint8_t reg;
		uint8_t val;
	};

	static const struct bl_reg bl_on_list[] = {
		{0x00, 0x00},	/* backlight off */
		{0x10, 0x00},	/* Brightness mode: PWM */
		{0x11, 0x04},	/* maxcurrent: 18ma */
		{0x12, 0x39},	/* pgen: 9.8kHz, full threshold */
		{0x13, 0x03},	/* boostfreq: 1MHz, bcomp option 1 */
		{0x14, 0xbf},	/* ov: 2v, all 6 current sinks enabled */
		{0x15, 0xc3},	/* smoothing step */
		{0x00, 0x01},	/* backlight on */
	};

	static const struct bl_reg bl_off_list[] = {
		{0x00, 0x00},	/* backlight off */
	};

	TegraI2c *backlight_i2c = get_i2c6();
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

static void *const disp_cmd = (void *)(uintptr_t)0x542000c8;

#define CTRL_MODE_MASK		(0x3 << 5)

static int smaug_display_stop(struct DisplayOps *me)
{
	writel(readl(disp_cmd) & ~CTRL_MODE_MASK, disp_cmd);
	return 0;
}

static DisplayOps smaug_display_ops = {
	.init = &tegra132_display_init,
	.backlight_update = &smaug_backlight_update,
	.stop = &smaug_display_stop,
};

static int display_setup(void)
{
	if (lib_sysinfo.framebuffer == NULL ||
		lib_sysinfo.framebuffer->physical_address == 0)
		return 0;

	display_set_ops(&smaug_display_ops);

	return 0;
}

INIT_FUNC(display_setup);

int board_draw_splash(uint32_t localize)
{
	int rv = 0;
	uint32_t size;
	uint8_t *buf = load_bitmap("splash.bmp", &size);
	if (!buf)
		return -1;
	/*
	 * need to draw using native drawing routine to make the image match
	 * pixel by pixel the animation following after depthcharge.
	 *
	 * TODO: compress the image so that we can store full screen image
	 * and avoid hardcoding the coordinate.
	 */
	if (dc_corebootfb_draw_bitmap(900, 760, buf))
		rv = -1;
	free(buf);
	return rv;
}
