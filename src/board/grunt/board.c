/*
 * Copyright 2017 Google Inc.
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
 */

#include <arch/msr.h>
#include <libpayload.h>

#include "base/init_funcs.h"
#include "base/list.h"
#include "config.h"
#include "drivers/bus/i2c/cros_ec_tunnel.h"
#include "drivers/bus/i2c/designware.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/ec/anx3429/anx3429.h"
#include "drivers/ec/ps8751/ps8751.h"
#include "drivers/flash/flash.h"
#include "drivers/flash/memmapped.h"
#include "drivers/gpio/kern.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/fch.h"
#include "drivers/soc/stoneyridge.h"
#include "drivers/sound/gpio_i2s.h"
#include "drivers/sound/max98357a.h"
#include "drivers/sound/route.h"
#include "drivers/storage/ahci.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/sdhci.h"
#include "drivers/storage/bayhub.h"
#include "drivers/tpm/cr50_i2c.h"
#include "drivers/tpm/tpm.h"
#include "drivers/video/display.h"
#include "drivers/bus/usb/usb.h"
#include "pci.h"
#include "vboot/util/flag.h"

#define HW_CONFIG_REG 0xc0010015
#define   HW_CONFIG_CPBDIS (1 << 25)

#define EMMC_SD_CLOCK_MIN	400000
#define EMMC_CLOCK_MAX		200000000
#define SD_CLOCK_MAX		52000000

#define FLASH_SIZE		0x1000000
#define FLASH_START		( 0xffffffff - FLASH_SIZE + 1 )

#define DA7219_I2C_ADDR		0x1a

#define DA7219_DAI_CLK_MODE	0x2b
#define   DAI_CLK_EN		0x80

/* cr50's interrupt in attached to GPIO_9 */
#define CR50_INT		9

#define BH720_PCI_VID		0x1217
#define BH720_PCI_DID		0x8620

#define GPIO_BACKLIGHT		133

/* GMMx1475C ACP_BT_UART_PAD_SEL */
#define GMMx1475C		0x1475c

#define EC_USB_PD_PORT_ANX3429	0
#define EC_I2C_PORT_ANX3429	1

#define EC_USB_PD_PORT_PS8751	1
#define EC_I2C_PORT_PS8751	2

int is_barla_alc5682(uint32_t id);
int is_treeya_alc5682(uint32_t id);

static int cr50_irq_status(void)
{
	static KernGpio *tpm_gpio;

	if (!tpm_gpio)
		tpm_gpio = new_kern_fch_gpio_latched(CR50_INT);

	return gpio_get(&tpm_gpio->ops);
}

/*
 * Force reset GMMx1475C ACP_BT_UART_PAD_SEL to 0 to enable bit-banging the
 * BT_I2S pins as GPIO.  This reset happens on reboot and entering G3, but NOT
 * in S5.
 */
static void audio_bt_i2s_setup(void)
{
	/* D1F0x24 == Graphics Memory Mapped Registers Base Address */
	uintptr_t gmm_base = pci_read_config32(PCI_DEV(0, 0x1, 0), 0x24);
	gmm_base &= 0xfffffff0;

	write32((void *)gmm_base + GMMx1475C, 0x00);
}

static int (*gpio_i2s_play)(struct SoundOps *me, uint32_t msec,
		uint32_t frequency);

static int amd_gpio_i2s_play(struct SoundOps *me, uint32_t msec,
		uint32_t frequency)
{
	int ret;
	uint64_t cur;

	cur = _rdmsr(HW_CONFIG_REG);

	/* Disable Core Boost while bit-banging I2S */
	_wrmsr(HW_CONFIG_REG, cur | HW_CONFIG_CPBDIS);

	ret = gpio_i2s_play(me, msec, frequency);

	/* Restore previous Core Boost setting */
	_wrmsr(HW_CONFIG_REG, cur);

	return ret;
}

static void audio_setup(uint32_t skuid)
{
	if (!is_barla_alc5682(skuid) || !is_treeya_alc5682(skuid)) {
		/* Setup da7219 on I2C0 */
		DesignwareI2c *i2c0 = new_designware_i2c(AP_I2C0_ADDR, 400000,
						 	 AP_I2C_CLK_MHZ);

		/* Clear DAI_CLK_MODE.dai_clk_en to set BCLK/WCLK pins as inputs */
		int ret = i2c_clear_bits(&i2c0->ops, DA7219_I2C_ADDR,
				 	 DA7219_DAI_CLK_MODE, DAI_CLK_EN);
		/*
		 * If we cannot clear DAI_CLK_EN, we may be fighting with it for
		 * control of BCLK/WCLK, so skip audio initialization.
		 */
		if (ret < 0) {
			printf("Failed to clear dai_clk_en (%d), skipping bit-bang i2s config\n",
			       ret);
			return;
		}
	}

	audio_bt_i2s_setup();

	KernGpio *i2s_bclk = new_kern_fch_gpio_output(140, 0);
	KernGpio *i2s_lrclk = new_kern_fch_gpio_output(144, 0);
	KernGpio *i2s2_data = new_kern_fch_gpio_output(143, 0);
	GpioI2s *i2s = new_gpio_i2s(
			&i2s_bclk->ops,		/* I2S Bit Clock GPIO */
			&i2s_lrclk->ops,	/* I2S Frame Sync GPIO */
			&i2s2_data->ops,	/* I2S Data GPIO */
			24000,			/* Sample rate, measured */
			2,			/* Channels */
			0x1FFF);		/* Volume */
	SoundRoute *sound_route = new_sound_route(&i2s->ops);

	/*
	 * Override gpio_i2s play() op with our own that disbles CPU boost
	 * before GPIO bit-banging I2S.
	 */
	gpio_i2s_play = i2s->ops.play;
	i2s->ops.play = amd_gpio_i2s_play;

	KernGpio *spk_pa_en = new_kern_fch_gpio_output(119, 0);
	max98357aCodec *speaker_amp = new_max98357a_codec(&spk_pa_en->ops);

	list_insert_after(&speaker_amp->component.list_node,
			  &sound_route->components);

	/* Disable BCLK buffer for GPIO beep */
	new_kern_fch_gpio_output(135, 0);

	sound_set_ops(&sound_route->ops);
}

static int grunt_backlight_update(DisplayOps *me, uint8_t enable)
{
	/* Backlight signal is active low */
	static KernGpio *backlight;

	if (!backlight)
		backlight = new_kern_fch_gpio_output(GPIO_BACKLIGHT, !enable);
	else
		backlight->ops.set(&backlight->ops, !enable);

	return 0;
}

static DisplayOps grunt_display_ops = {
	.backlight_update = &grunt_backlight_update,
};

int is_treeya(uint32_t id)
{
	if (((id >= 0xa0) && (id <= 0xaf)) || ((id >= 0xb8) && (id <= 0xbf)))
		return 1;
	else
		return 0;
}

int is_nuwani(uint32_t id)
{
	if ((id < 0xd0) || (id > 0xdf))
		return 0;
	else
		return 1;
}

int is_barla_alc5682(uint32_t id)
{
	switch(id) {
	case 0x2c:
	case 0x2d:
	case 0x2e:
	case 0x2f:
		return 1;
	default:
		return 0;

	}
}

int is_treeya_alc5682(uint32_t id)
{
	switch(id) {
	case 0xbe:
	case 0xbf:
		return 1;
	default:
		return 0;
	}
}

static int board_setup(void)
{
	CrosECTunnelI2c *cros_ec_i2c_tunnel;

	sysinfo_install_flags(NULL);
	CrosEcLpcBus *cros_ec_lpc_bus =
		new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, 0, NULL);
	register_vboot_ec(&cros_ec->vboot, 0);

	flag_replace(FLAG_LIDSW, cros_ec_lid_switch_flag());
	flag_replace(FLAG_PWRSW, cros_ec_power_btn_flag());

	/* programmables downstream from the EC */
	cros_ec_i2c_tunnel =
		new_cros_ec_tunnel_i2c(cros_ec, EC_I2C_PORT_PS8751);
	Ps8751 *ps8751 = new_ps8751(cros_ec_i2c_tunnel, EC_USB_PD_PORT_PS8751);
	register_vboot_aux_fw(&ps8751->fw_ops);

	/* Treeya variant use anx3447 which do not have on board OCM and does
	 * not support firmware update. Treeya && Treeya360 sku id range is:
	 * 0xa0 - 0xaf & 0xb8 - 0xbf.
	 * Nuwani and Nuwani360 sku id range is: 0xd0 - 0xdf
	 */
	if (!is_treeya(lib_sysinfo.sku_id) && !is_nuwani(lib_sysinfo.sku_id)) {
		cros_ec_i2c_tunnel =
			new_cros_ec_tunnel_i2c(cros_ec, EC_I2C_PORT_ANX3429);
		Anx3429 *anx3429 =
			new_anx3429(cros_ec_i2c_tunnel, EC_USB_PD_PORT_ANX3429);
		register_vboot_aux_fw(&anx3429->fw_ops);
	}

	flash_set_ops(&new_mem_mapped_flash(FLASH_START, FLASH_SIZE)->ops);

	audio_setup(lib_sysinfo.sku_id);

	SdhciHost *emmc = NULL;
	/* The proto version of Grunt has the FT4's SDHCI pins wired up to
	 * the eMMC part.  All other versions of Grunt (are planned to) use
	 * the BH720 SDHCI controller for EMMC and use the FT4 SDHCI pins for
	 * the SD connector.
	 */
	if (lib_sysinfo.board_id > 0) {
		pcidev_t pci_dev;
		if (pci_find_device(BH720_PCI_VID, BH720_PCI_DID, &pci_dev)) {
			emmc = new_bayhub_sdhci_host(pci_dev,
				SDHCI_PLATFORM_CLEAR_TRANSFER_BEFORE_CMD,
				EMMC_SD_CLOCK_MIN, EMMC_CLOCK_MAX);
			emmc->name = "eMMC";
		} else {
			printf("Failed to find BH720 with VID/DID %04x:%04x\n",
				BH720_PCI_VID, BH720_PCI_DID);
		}

		SdhciHost *sd;
		sd = new_pci_sdhci_host(PCI_DEV(0, 0x14, 7),
				SDHCI_PLATFORM_REMOVABLE |
				SDHCI_PLATFORM_CLEAR_TRANSFER_BEFORE_CMD,
				EMMC_SD_CLOCK_MIN, SD_CLOCK_MAX);
		sd->name = "SD";
		list_insert_after(&sd->mmc_ctrlr.ctrlr.list_node,
				&removable_block_dev_controllers);
	} else {
		emmc = new_pci_sdhci_host(PCI_DEV(0, 0x14, 7),
				SDHCI_PLATFORM_NO_EMMC_HS200 |
				SDHCI_PLATFORM_CLEAR_TRANSFER_BEFORE_CMD,
				EMMC_SD_CLOCK_MIN, EMMC_CLOCK_MAX);
	}
	if (emmc) {
		list_insert_after(&emmc->mmc_ctrlr.ctrlr.list_node,
				&fixed_block_dev_controllers);
	}

	/* Setup h1 on I2C1 */
	DesignwareI2c *i2c_h1 = new_designware_i2c(
		AP_I2C1_ADDR, 400000, AP_I2C_CLK_MHZ);
	tpm_set_ops(&new_cr50_i2c(&i2c_h1->ops, 0x50,
				  &cr50_irq_status)->base.ops);

	power_set_ops(&kern_power_ops);
	display_set_ops(&grunt_display_ops);

	return 0;
}

INIT_FUNC(board_setup);
