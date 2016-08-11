/*
 * Copyright 2014 Rockchip Electronics Co., Ltd.
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

#include <arch/io.h>

#include "base/init_funcs.h"
#include "base/container_of.h"
#include "boot/fit.h"
#include "drivers/gpio/rockchip.h"
#include "drivers/bus/i2c/rockchip.h"
#include "drivers/flash/spi.h"
#include "drivers/bus/spi/rockchip.h"
#include "drivers/tpm/slb9635_i2c.h"
#include "drivers/tpm/tpm.h"
#include "drivers/power/rk808.h"
#include "drivers/power/sysinfo.h"
#include "drivers/sound/dummy.h"
#include "drivers/storage/dw_mmc.h"
#include "drivers/storage/rk_dwmmc.h"
#include "drivers/video/display.h"

#include "drivers/gpio/sysinfo.h"
#include "vboot/util/flag.h"
#include "drivers/bus/i2s/rockchip.h"

#include "drivers/bus/usb/usb.h"

static void install_phys_presence_flag(void)
{
	GpioOps *phys_presence = sysinfo_lookup_gpio(
			"recovery", 1, new_rk_gpio_input_from_coreboot);

	if (!phys_presence) {
		printf("%s failed retrieving recovery GPIO\n", __func__);
		return;
	}
	flag_install(FLAG_PHYS_PRESENCE, phys_presence);
}

typedef struct {
	DisplayOps ops;

	GpioOps *ready;		/* Wireless icon, white */
	GpioOps *ready2;	/* Wireless icon, orange */
	GpioOps *syncing;	/* Refresh icon, white */
	GpioOps *error;		/* Warning icon, orange */
} RialtoDisplayOps;

static int rialto_leds_display_screen(DisplayOps *me,
				      enum VbScreenType_t screen_type) {
	RialtoDisplayOps *leds = container_of(me, RialtoDisplayOps, ops);

	switch(screen_type) {
	case VB_SCREEN_BLANK:
		gpio_set(leds->ready,   1);
		gpio_set(leds->ready2,  1);
		gpio_set(leds->syncing, 1);
		gpio_set(leds->error,   1);
		break;

	case VB_SCREEN_DEVELOPER_WARNING:
		gpio_set(leds->ready,   0);
		gpio_set(leds->ready2,  0);
		gpio_set(leds->syncing, 1);
		gpio_set(leds->error,   1);
		break;

	case VB_SCREEN_RECOVERY_REMOVE:
		gpio_set(leds->ready,   0);
		gpio_set(leds->ready2,  1);
		gpio_set(leds->syncing, 0);
		gpio_set(leds->error,   1);
		break;

	case VB_SCREEN_RECOVERY_INSERT:
		gpio_set(leds->ready,   0);
		gpio_set(leds->ready2,  0);
		gpio_set(leds->syncing, 0);
		gpio_set(leds->error,   1);
		break;

	case VB_SCREEN_DEVELOPER_TO_NORM:
		/* fall through */
	case VB_SCREEN_RECOVERY_TO_DEV:
		gpio_set(leds->ready,   1);
		gpio_set(leds->ready2,  0);
		gpio_set(leds->syncing, 1);
		gpio_set(leds->error,   1);
		break;

	case VB_SCREEN_TO_NORM_CONFIRMED:
		/* same as blank */
		gpio_set(leds->ready,   1);
		gpio_set(leds->ready2,  1);
		gpio_set(leds->syncing, 1);
		gpio_set(leds->error,   1);
		break;

	case VB_SCREEN_RECOVERY_NO_GOOD:
		gpio_set(leds->ready,   1);
		gpio_set(leds->ready2,  1);
		gpio_set(leds->syncing, 0);
		gpio_set(leds->error,   1);
		break;

	case VB_SCREEN_DEVELOPER_EGG:
		/* fall through */
	default:
		printf("%s: no led pattern defined for screen %d\n",
			__func__, screen_type);
		return -1;
	}
	return 0;
}

RialtoDisplayOps *new_rialto_leds(void) {
	RialtoDisplayOps *leds = xzalloc(sizeof(*leds));

	/* The following are active high */
	leds->ready   = &new_rk_gpio_output(GPIO(7, A, 0))->ops; /* LED_READY */
	leds->ready2  = &new_rk_gpio_output(GPIO(7, B, 5))->ops; /* Ready2_LED */
	leds->syncing = &new_rk_gpio_output(GPIO(7, B, 3))->ops; /* LED_SYNCING */
	leds->error   = &new_rk_gpio_output(GPIO(7, B, 7))->ops; /* LED_ERROR */

	leds->ops.display_screen = rialto_leds_display_screen;

	return leds;
}

uint16_t dev_mode_music[] = {
	0x5324, 0x444e, /* "$SND" signature */
	0xb2fb, 0x2650, /* crc32 over count & all notes */
	10, 0,          /* number of notes */
	20000, 0,       /* 20 seconds */
	1000, 400,      /* two long beeps, to "qualify" as a song */
	1000, 0,
	1000, 400,
	7000, 0,        /* 30 seconds so far */
	30000, 0,       /* another 2.5 minutes */
	30000, 0,
	30000, 0,
	30000, 0,
	30000, 0        /* 3 minutes total */
};

void *VbExGetMusicPtr(void) {
	return &dev_mode_music;
}

uint32_t VbExMaxMusicSize(void) {
	return sizeof(dev_mode_music);
}

static int board_setup(void)
{
	RialtoDisplayOps *leds;

	RkSpi *spi2 = new_rockchip_spi(0xff130000);
	flash_set_ops(&new_spi_flash(&spi2->ops)->ops);

	sysinfo_install_flags(new_rk_gpio_input_from_coreboot);

	RkI2c *i2c1 = new_rockchip_i2c((void *)0xff140000);
	tpm_set_ops(&new_slb9635_i2c(&i2c1->ops, 0x20)->base.ops);

	sound_set_ops(new_dummy_sound());

	RkI2c *i2c0 = new_rockchip_i2c((void *)0xff650000);
	Rk808Pmic *pmic = new_rk808_pmic(&i2c0->ops, 0x1b);
	SysinfoResetPowerOps *power = new_sysinfo_reset_power_ops(&pmic->ops,
			new_rk_gpio_output_from_coreboot);
	power_set_ops(&power->ops);

	DwmciHost *emmc = new_rkdwmci_host(0xff0f0000, 594000000, 8, 0, NULL);
	list_insert_after(&emmc->mmc.ctrlr.list_node,
			  &fixed_block_dev_controllers);

	leds = new_rialto_leds();
	display_set_ops(&(leds->ops));

	UsbHostController *usb_host1 = new_usb_hc(DWC2, 0xff540000);
	list_insert_after(&usb_host1->list_node, &usb_host_controllers);

	UsbHostController *usb_otg = new_usb_hc(DWC2, 0xff580000);
	list_insert_after(&usb_otg->list_node, &usb_host_controllers);

	// Read the current value of the recovery button for confirmation
	// when transitioning between normal and dev mode.
	flag_replace(FLAG_RECSW, sysinfo_lookup_gpio("recovery",
				1, new_rk_gpio_input_from_coreboot));

	/* Lid always open for now. */
	flag_replace(FLAG_LIDSW, new_gpio_high());

	/* Follow Storm to use recovery button as Ctrl-U. */
	install_phys_presence_flag();

	return 0;
}

INIT_FUNC(board_setup);
