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

#include <libpayload.h>

#include "base/init_funcs.h"
#include "drivers/bus/spi/ipq806x.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/gpio/gpio.h"
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

	return 0;
}

INIT_FUNC(board_setup);
