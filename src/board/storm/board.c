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

#include "base/init_funcs.h"
#include "drivers/bus/i2c/ipq806x_gsbi.h"
#include "drivers/bus/i2c/ipq806x.h"
#include "drivers/bus/spi/ipq806x.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/gpio/gpio.h"
#include "drivers/tpm/slb9635_i2c.h"
#include "drivers/tpm/tpm.h"
#include "vboot/util/flag.h"

typedef struct
{
	GpioOps fake_ops;
	int	fake_value;
} FakeGpio;

static int get_fake_gpio(struct GpioOps *me)
{
	FakeGpio *fgpio = container_of(me, FakeGpio, fake_ops);

	return fgpio->fake_value;
}

static FakeGpio fake_gpios[] = 	{
	{ .fake_ops = { .get = get_fake_gpio, }, 1 }, /* FLAG_WPSW */
	{ .fake_ops = { .get = get_fake_gpio, }, 0 }, /* FLAG_RECSW */
	{ .fake_ops = { .get = get_fake_gpio, }, 1 }, /* FLAG_DEVSW */
	{ .fake_ops = { .get = get_fake_gpio, }, 1 }, /* FLAG_LIDSW */
	{ .fake_ops = { .get = get_fake_gpio, }, 0 }, /* FLAG_PWRSW */
};

static int board_setup(void)
{
	int i;

	SpiController *spi = new_spi(0, 0);
	flash_set_ops(&new_spi_flash(&spi->ops, 0x800000)->ops);

	UsbHostController *usb_host1 = new_usb_hc(XHCI, 0x11000000);

	list_insert_after(&usb_host1->list_node, &usb_host_controllers);

	for (i = 0; i < ARRAY_SIZE(fake_gpios); i++)
		flag_install(i, &fake_gpios[i].fake_ops);

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
