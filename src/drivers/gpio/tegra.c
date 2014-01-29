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
#include <stdint.h>

#include "base/container_of.h"
#include "drivers/gpio/tegra.h"

enum {
	GPIO_GPIOS_PER_PORT = 8,
	GPIO_PORTS_PER_BANK = 4,
	GPIO_BANKS = 8,
};

typedef struct {
	// Values
	uint32_t config[GPIO_PORTS_PER_BANK];
	uint32_t out_enable[GPIO_PORTS_PER_BANK];
	uint32_t out_value[GPIO_PORTS_PER_BANK];
	uint32_t in_value[GPIO_PORTS_PER_BANK];
	uint32_t int_status[GPIO_PORTS_PER_BANK];
	uint32_t int_enable[GPIO_PORTS_PER_BANK];
	uint32_t int_level[GPIO_PORTS_PER_BANK];
	uint32_t int_clear[GPIO_PORTS_PER_BANK];

	// Masks
	uint32_t config_mask[GPIO_PORTS_PER_BANK];
	uint32_t out_enable_mask[GPIO_PORTS_PER_BANK];
	uint32_t out_value_mask[GPIO_PORTS_PER_BANK];
	uint32_t in_value_mask[GPIO_PORTS_PER_BANK];
	uint32_t int_status_mask[GPIO_PORTS_PER_BANK];
	uint32_t int_enable_mask[GPIO_PORTS_PER_BANK];
	uint32_t int_level_mask[GPIO_PORTS_PER_BANK];
	uint32_t int_clear_mask[GPIO_PORTS_PER_BANK];
} GpioBank;

// If this changes between SOCs but this driver would otherwise work, we'll
// have to parameterize the address.
static GpioBank * const gpio_banks = (void *)0x6000d000;
static uint32_t * const pinmux_regs = (void *)0x70003000;

static int tegra_gpio_get(GpioOps *me)
{
	TegraGpio *gpio = container_of(me, TegraGpio, ops);
	int bank = gpio->port / GPIO_PORTS_PER_BANK;
	int port = gpio->port - bank * GPIO_PORTS_PER_BANK;

	uint32_t reg = readl(&gpio_banks[bank].in_value[port]);
	return (reg & (1 << gpio->index)) != 0;
}

static int tegra_gpio_set(GpioOps *me, unsigned value)
{
	TegraGpio *gpio = container_of(me, TegraGpio, ops);
	int bank = gpio->port / GPIO_PORTS_PER_BANK;
	int port = gpio->port - bank * GPIO_PORTS_PER_BANK;
	uint32_t reg = readl(&gpio_banks[bank].out_value[port]);
	uint32_t new_reg = value ? (reg | (0x1 << gpio->index)) :
				   (reg & ~(0x1 << gpio->index));

	if (new_reg != reg)
		writel(new_reg, &gpio_banks[bank].out_value[port]);

	return 0;
}

static TegraGpio *new_tegra_gpio(TegraGpioPort port, unsigned index,
				 unsigned pinmux)
{
	die_if(port < 0 || port >= GPIO_NUM_PORTS, "Bad GPIO port %d.\n", port);
	die_if(index >= GPIO_GPIOS_PER_PORT, "Bad GPIO index %d.\n", index);

	TegraGpio *gpio = xzalloc(sizeof(*gpio));
	gpio->port = port;
	gpio->index = index;
	gpio->pinmux = pinmux;

	return gpio;
}

TegraGpio *new_tegra_gpio_input(TegraGpioPort port, unsigned index,
				unsigned pinmux)
{
	TegraGpio *gpio = new_tegra_gpio(port, index, pinmux);
	gpio->ops.get = &tegra_gpio_get;
	return gpio;
}

TegraGpio *new_tegra_gpio_output(TegraGpioPort port, unsigned index,
				 unsigned pinmux)
{
	TegraGpio *gpio = new_tegra_gpio(port, index, pinmux);
	gpio->ops.set = &tegra_gpio_set;
	return gpio;
}

static void tegra_pinmux_set_pull(int pin_index, uint32_t config)
{
	uint32_t reg = readl(&pinmux_regs[pin_index]);

	reg = (reg & ~PINMUX_PULL_MASK) | (config & PINMUX_PULL_MASK);
	writel(reg, &pinmux_regs[pin_index]);
}

int tegra_gpio_get_in_tristate_values(TegraGpio *gpio[], int num_gpio,
				      int value[])
{
	/*
	 * GPIOs which are tied to stronger external pull up or pull down
	 * will stay there regardless of the internal pull up or pull
	 * down setting.
	 *
	 * GPIOs which are floating will go to whatever level they're
	 * internally pulled to.
	 */

	int temp;
	int index;

	/* Enable internal pull up */
	for (index = 0; index < num_gpio; ++index)
		tegra_pinmux_set_pull(gpio[index]->pinmux, PINMUX_PULL_UP);

	/* Wait until signals become stable */
	udelay(10);

	/* Get gpio values at internal pull up */
	for (index = 0; index < num_gpio; ++index)
		value[index] = gpio[index]->ops.get(&gpio[index]->ops);

	/* Enable internal pull down */
	for (index = 0; index < num_gpio; ++index)
		tegra_pinmux_set_pull(gpio[index]->pinmux, PINMUX_PULL_DOWN);

	/* Wait until signals become stable */
	udelay(10);

	/*
	 * Get gpio values at internal pull down.
	 * Compare with gpio pull up value and then
	 * determine a gpio final value/state:
	 *  0: pull down
	 *  1: pull up
	 *  2: floating
	 */
	for (index = 0; index < num_gpio; ++index) {
		temp = gpio[index]->ops.get(&gpio[index]->ops);
		value[index] = ((value[index] ^ temp) << 1) | temp;
	}

	/* Disable pull up / pull down to conserve power */
	for (index = 0; index < num_gpio; ++index)
		tegra_pinmux_set_pull(gpio[index]->pinmux, PINMUX_PULL_NONE);

	return 0;
}
