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

#include <assert.h>
#include <libpayload.h>
#include <stdint.h>

#include "base/container_of.h"
#include "drivers/gpio/exynos5250.h"
#include "drivers/gpio/gpio.h"

typedef struct __attribute__((packed)) GpioRegs
{
	uint32_t con;
	uint32_t dat;
	uint32_t pud;
	uint32_t drv;
	uint32_t conpdn;
	uint32_t pudpdn;
} GpioRegs;

typedef struct GpioBank
{
	int num_gpios;
	GpioRegs *regs;
} GpioBank;

typedef struct GpioGroup
{
	int num_banks;
	GpioBank *banks[8];
} GpioGroup;

static GpioBank bank_a0 = { 8, (void *)(uintptr_t)0x11400000 };
static GpioBank bank_a1 = { 6, (void *)(uintptr_t)0x11400020 };
static GpioBank bank_a2 = { 8, (void *)(uintptr_t)0x11400040 };

static GpioBank bank_b0 = { 5, (void *)(uintptr_t)0x11400060 };
static GpioBank bank_b1 = { 5, (void *)(uintptr_t)0x11400080 };
static GpioBank bank_b2 = { 4, (void *)(uintptr_t)0x114000a0 };
static GpioBank bank_b3 = { 4, (void *)(uintptr_t)0x114000c0 };

static GpioBank bank_c0 = { 7, (void *)(uintptr_t)0x114000e0 };
static GpioBank bank_c1 = { 4, (void *)(uintptr_t)0x11400100 };
static GpioBank bank_c2 = { 7, (void *)(uintptr_t)0x11400120 };
static GpioBank bank_c3 = { 7, (void *)(uintptr_t)0x11400140 };
static GpioBank bank_c4 = { 7, (void *)(uintptr_t)0x114002e0 };

static GpioBank bank_d0 = { 4, (void *)(uintptr_t)0x11400160 };
static GpioBank bank_d1 = { 8, (void *)(uintptr_t)0x11400180 };

static GpioBank bank_e0 = { 8, (void *)(uintptr_t)0x13400000 };
static GpioBank bank_e1 = { 2, (void *)(uintptr_t)0x13400020 };

static GpioBank bank_f0 = { 4, (void *)(uintptr_t)0x13400040 };
static GpioBank bank_f1 = { 4, (void *)(uintptr_t)0x13400060 };

static GpioBank bank_g0 = { 8, (void *)(uintptr_t)0x13400080 };
static GpioBank bank_g1 = { 8, (void *)(uintptr_t)0x134000a0 };
static GpioBank bank_g2 = { 2, (void *)(uintptr_t)0x134000c0 };

static GpioBank bank_h0 = { 4, (void *)(uintptr_t)0x134000e0 };
static GpioBank bank_h1 = { 8, (void *)(uintptr_t)0x13400100 };

static GpioBank bank_v0 = { 8, (void *)(uintptr_t)0x10d10000 };
static GpioBank bank_v1 = { 8, (void *)(uintptr_t)0x10d10020 };
static GpioBank bank_v2 = { 8, (void *)(uintptr_t)0x10d10060 };
static GpioBank bank_v3 = { 8, (void *)(uintptr_t)0x10d10080 };
static GpioBank bank_v4 = { 2, (void *)(uintptr_t)0x10d100c0 };

static GpioBank bank_x0 = { 8, (void *)(uintptr_t)0x11400c00 };
static GpioBank bank_x1 = { 8, (void *)(uintptr_t)0x11400c20 };
static GpioBank bank_x2 = { 8, (void *)(uintptr_t)0x11400c40 };
static GpioBank bank_x3 = { 8, (void *)(uintptr_t)0x11400c60 };

static GpioBank bank_y0 = { 6, (void *)(uintptr_t)0x114001a0 };
static GpioBank bank_y1 = { 4, (void *)(uintptr_t)0x114001c0 };
static GpioBank bank_y2 = { 6, (void *)(uintptr_t)0x114001e0 };
static GpioBank bank_y3 = { 8, (void *)(uintptr_t)0x11400200 };
static GpioBank bank_y4 = { 8, (void *)(uintptr_t)0x11400220 };
static GpioBank bank_y5 = { 8, (void *)(uintptr_t)0x11400240 };
static GpioBank bank_y6 = { 8, (void *)(uintptr_t)0x11400260 };

static GpioBank bank_z =  { 7, (void *)(uintptr_t)0x03860000 };

GpioGroup groups[] = {
	[GPIO_A] = { 3, { &bank_a0, &bank_a1, &bank_a2 }},
	[GPIO_B] = { 4, { &bank_b0, &bank_b1, &bank_b2, &bank_b3 }},
	[GPIO_C] = { 5, { &bank_c0, &bank_c1, &bank_c2, &bank_c3, &bank_c4 }},
	[GPIO_D] = { 2, { &bank_d0, &bank_d1 }},
	[GPIO_E] = { 2, { &bank_e0, &bank_e1 }},
	[GPIO_F] = { 2, { &bank_f0, &bank_f1 }},
	[GPIO_G] = { 3, { &bank_g0, &bank_g1, &bank_g2 }},
	[GPIO_H] = { 2, { &bank_h0, &bank_h1, }},
	[GPIO_V] = { 5, { &bank_v0, &bank_v1, &bank_v2, &bank_v3, &bank_v4 }},
	[GPIO_X] = { 4, { &bank_x0, &bank_x1, &bank_x2, &bank_x3 }},
	[GPIO_Y] = { 7, { &bank_y0, &bank_y1, &bank_y2, &bank_y3, &bank_y4,
			  &bank_y5, &bank_y6 }},
	[GPIO_Z] = { 1, { &bank_z }}
};

static GpioRegs *exynos5250_get_regs(Exynos5250Gpio *gpio)
{
	GpioGroup *group = &groups[gpio->group];
	GpioBank *bank = group->banks[gpio->bank];
	return bank->regs;
}

static int exynos5250_gpio_use(Exynos5250Gpio *me, unsigned use)
{
	GpioRegs *regs = exynos5250_get_regs(me);

	uint32_t con = readl(&regs->con);
	con &= ~(0xf << (me->index * 4));
	con |= ((use & 0xf) << (me->index * 4));
	writel(con, &regs->con);

	return 0;
}

static int exynos5250_gpio_set_pud(Exynos5250Gpio *me, unsigned value)
{
	GpioRegs *regs = exynos5250_get_regs(me);

	uint32_t pud = readl(&regs->pud);
	pud &= ~(0x3 << (me->index * 2));
	pud |= ((value & 0x3) << (me->index * 2));
	writel(pud, &regs->pud);

	return 0;
}

static int exynos5250_gpio_get_value(GpioOps *me)
{
	assert(me);
	Exynos5250Gpio *gpio = container_of(me, Exynos5250Gpio, ops);
	GpioRegs *regs = exynos5250_get_regs(gpio);

	if (!gpio->dir_set) {
		if (gpio->use(gpio, 0))
			return -1;
		gpio->dir_set = 1;
	}

	return (readl(&regs->dat) >> gpio->index) & 0x1;
}

static int exynos5250_gpio_set_value(GpioOps *me, unsigned value)
{
	assert(me);
	Exynos5250Gpio *gpio = container_of(me, Exynos5250Gpio, ops);
	GpioRegs *regs = exynos5250_get_regs(gpio);

	if (!gpio->dir_set) {
		if (gpio->use(gpio, 1))
			return -1;
		gpio->dir_set = 1;
	}

	uint32_t dat = readl(&regs->dat);
	dat &= ~(0x1 << gpio->index);
	dat |= ((value & 0x1) << gpio->index);
	writel(dat, &regs->dat);
	return 0;
}

Exynos5250Gpio *new_exynos5250_gpio(unsigned group, unsigned bank,
				    unsigned index)
{
	die_if(group >= ARRAY_SIZE(groups) ||
	       bank >= groups[group].num_banks ||
	       index >= groups[group].banks[bank]->num_gpios,
	       "GPIO parameters (%d, %d, %d) out of bounds.\n",
	       group, bank, index);

	Exynos5250Gpio *gpio = xzalloc(sizeof(*gpio));
	gpio->use = &exynos5250_gpio_use;
	gpio->pud = &exynos5250_gpio_set_pud;
	gpio->group = group;
	gpio->bank = bank;
	gpio->index = index;
	return gpio;
}

Exynos5250Gpio *new_exynos5250_gpio_input(unsigned group, unsigned bank,
					  unsigned index)
{
	Exynos5250Gpio *gpio = new_exynos5250_gpio(group, bank, index);
	gpio->ops.get = &exynos5250_gpio_get_value;
	return gpio;
}

Exynos5250Gpio *new_exynos5250_gpio_output(unsigned group, unsigned bank,
					   unsigned index)
{
	Exynos5250Gpio *gpio = new_exynos5250_gpio(group, bank, index);
	gpio->ops.set = &exynos5250_gpio_set_value;
	return gpio;
}
