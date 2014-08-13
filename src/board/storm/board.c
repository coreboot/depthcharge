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

#include <config.h>
#include <libpayload.h>
#include <sysinfo.h>
#include <stdio.h>

#include "base/init_funcs.h"
#include "drivers/bus/i2c/ipq806x_gsbi.h"
#include "drivers/bus/i2c/ipq806x.h"
#include "drivers/bus/spi/ipq806x.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/gpio/gpio.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/tpm/slb9635_i2c.h"
#include "drivers/tpm/tpm.h"
#include "vboot/util/flag.h"
#include "drivers/gpio/ipq806x.h"
#include "drivers/storage/ipq806x_mmc.h"

#define GPIO_SDCC_FUNC_VAL      2

#define MSM_SDC1_BASE		0x12400000

/* MMC bus GPIO assignments. */
enum storm_emmc_gpio {
	SDC1_DATA7 = 38,
	SDC1_DATA6 = 39,
	SDC1_DATA3 = 40,
	SDC1_DATA2 = 41,
	SDC1_CLK = 42,
	SDC1_DATA1 = 43,
	SDC1_DATA0 = 44,
	SDC1_CMD = 45,
	SDC1_DATA5 = 46,
	SDC1_DATA4 = 47,
};

/* Storm GPIO access wrapper. */
typedef struct
{
	GpioOps gpio_ops;	/* Depthcharge GPIO API wrapper. */
	struct cb_gpio *cbgpio;	/* GPIO description. */

} StormGpio;

static int get_gpio(struct GpioOps *me)
{
	unsigned value;
	StormGpio *gpio = container_of(me, StormGpio, gpio_ops);

	value = gpio_get_in_value(gpio->cbgpio->port);
	if (gpio->cbgpio->polarity == CB_GPIO_ACTIVE_LOW)
		value = !value;

	return value;
}

static StormGpio phys_presence_flag = {
	.gpio_ops = { .get = get_gpio }
};

static void install_phys_presence_flag(void)
{
	phys_presence_flag.cbgpio = sysinfo_lookup_gpio("recovery");

	if (!phys_presence_flag.cbgpio) {
		printf("%s failed retrieving recovery GPIO\n", __func__);
		return;
	}

	flag_install(FLAG_PHYS_PRESENCE, &phys_presence_flag.gpio_ops);
}

void board_mmc_gpio_config(void)
{
	unsigned i;
	unsigned char gpio_config_arr[] = {
		SDC1_DATA7, SDC1_DATA6, SDC1_DATA3,
		SDC1_DATA2, SDC1_DATA1, SDC1_DATA0,
		SDC1_CMD, SDC1_DATA5, SDC1_DATA4};

	gpio_tlmm_config_set(SDC1_CLK, GPIO_SDCC_FUNC_VAL,
		GPIO_PULL_DOWN, GPIO_16MA, 1);

	for (i = 0; i < ARRAY_SIZE(gpio_config_arr); i++) {
		gpio_tlmm_config_set(gpio_config_arr[i],
		GPIO_SDCC_FUNC_VAL, GPIO_PULL_UP, GPIO_10MA, 1);
	}
}

static int board_setup(void)
{
	sysinfo_install_flags();

	install_phys_presence_flag();

	SpiController *spi = new_spi(0, 0);
	flash_set_ops(&new_spi_flash(&spi->ops, 0x800000)->ops);

	UsbHostController *usb_host1 = new_usb_hc(XHCI, 0x11000000);

	list_insert_after(&usb_host1->list_node, &usb_host_controllers);

	QcomMmcHost *mmc = new_qcom_mmc_host(1, MSM_SDC1_BASE, 8);
	if (!mmc)
		return -1;

	list_insert_after(&mmc->mmc.ctrlr.list_node,
			  &fixed_block_dev_controllers);

	Ipq806xI2c *i2c = new_ipq806x_i2c(GSBI_ID_1);
	tpm_set_ops(&new_slb9635_i2c(&i2c->ops, 0x20)->base.ops);

	return 0;
}

int get_mach_id(void)
{
	int i;
	struct cb_mainboard *mainboard = lib_sysinfo.mainboard;
	const char *part_number = (const char *)mainboard->strings +
		mainboard->part_number_idx;

	struct PartDescriptor {
		const char *part_name;
		int mach_id;
	} parts[] = {
		{"Storm", 4936},
		{"AP148", CONFIG_MACHID},
	};

	for (i = 0; i < ARRAY_SIZE(parts); i++) {
		if (!strncmp(parts[i].part_name,
			     part_number, strlen(parts[i].part_name))) {
			return parts[i].mach_id;
		}
	}

	return -1;
}

INIT_FUNC(board_setup);
