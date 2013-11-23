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
#include "drivers/gpio/gpio.h"
#include "drivers/gpio/exynos5420.h"

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

static GpioBank bank_a0 = { 8, (void *)(uintptr_t)0x14010000 };
static GpioBank bank_a1 = { 6, (void *)(uintptr_t)0x14010020 };
static GpioBank bank_a2 = { 8, (void *)(uintptr_t)0x14010040 };

static GpioBank bank_b0 = { 5, (void *)(uintptr_t)0x14010060 };
static GpioBank bank_b1 = { 5, (void *)(uintptr_t)0x14010080 };
static GpioBank bank_b2 = { 4, (void *)(uintptr_t)0x140100a0 };
static GpioBank bank_b3 = { 8, (void *)(uintptr_t)0x140100c0 };
static GpioBank bank_b4 = { 2, (void *)(uintptr_t)0x140100e0 };

static GpioBank bank_c0 = { 8, (void *)(uintptr_t)0x13410000 };
static GpioBank bank_c1 = { 8, (void *)(uintptr_t)0x13410020 };
static GpioBank bank_c2 = { 7, (void *)(uintptr_t)0x13410040 };
static GpioBank bank_c3 = { 4, (void *)(uintptr_t)0x13410060 };
static GpioBank bank_c4 = { 2, (void *)(uintptr_t)0x13410080 };

static GpioBank bank_d1 = { 8, (void *)(uintptr_t)0x134100a0 };

static GpioBank bank_e0 = { 8, (void *)(uintptr_t)0x14000000 };
static GpioBank bank_e1 = { 2, (void *)(uintptr_t)0x14000020 };

static GpioBank bank_f0 = { 6, (void *)(uintptr_t)0x14000040 };
static GpioBank bank_f1 = { 8, (void *)(uintptr_t)0x14000060 };

static GpioBank bank_g0 = { 8, (void *)(uintptr_t)0x14000080 };
static GpioBank bank_g1 = { 8, (void *)(uintptr_t)0x140000a0 };
static GpioBank bank_g2 = { 2, (void *)(uintptr_t)0x140000c0 };

static GpioBank bank_h0 = { 8, (void *)(uintptr_t)0x14010100 };

static GpioBank bank_j4 = { 4, (void *)(uintptr_t)0x140000e0 };

static GpioBank bank_x0 = { 8, (void *)(uintptr_t)0x13400c00 };
static GpioBank bank_x1 = { 8, (void *)(uintptr_t)0x13400c20 };
static GpioBank bank_x2 = { 8, (void *)(uintptr_t)0x13400c40 };
static GpioBank bank_x3 = { 8, (void *)(uintptr_t)0x13400c60 };

static GpioBank bank_y0 = { 6, (void *)(uintptr_t)0x134100c0 };
static GpioBank bank_y1 = { 4, (void *)(uintptr_t)0x134100e0 };
static GpioBank bank_y2 = { 6, (void *)(uintptr_t)0x13410100 };
static GpioBank bank_y3 = { 8, (void *)(uintptr_t)0x13410120 };
static GpioBank bank_y4 = { 8, (void *)(uintptr_t)0x13410140 };
static GpioBank bank_y5 = { 8, (void *)(uintptr_t)0x13410160 };
static GpioBank bank_y6 = { 8, (void *)(uintptr_t)0x13410180 };
static GpioBank bank_y7 = { 8, (void *)(uintptr_t)0x13400000 };

static GpioBank bank_z =  { 7, (void *)(uintptr_t)0x03860000 };

GpioGroup groups[] = {
	[GPIO_A] = { 3, { &bank_a0, &bank_a1, &bank_a2 }},
	[GPIO_B] = { 5, { &bank_b0, &bank_b1, &bank_b2, &bank_b3, &bank_b4 }},
	[GPIO_C] = { 5, { &bank_c0, &bank_c1, &bank_c2, &bank_c3, &bank_c4 }},
	[GPIO_D] = { 2, { NULL,     &bank_d1 }},
	[GPIO_E] = { 2, { &bank_e0, &bank_e1 }},
	[GPIO_F] = { 2, { &bank_f0, &bank_f1 }},
	[GPIO_G] = { 3, { &bank_g0, &bank_g1, &bank_g2 }},
	[GPIO_H] = { 1, { &bank_h0, }},
	[GPIO_J] = { 5, { NULL,     NULL,     NULL,     NULL,     &bank_j4 }},
	[GPIO_X] = { 4, { &bank_x0, &bank_x1, &bank_x2, &bank_x3 }},
	[GPIO_Y] = { 8, { &bank_y0, &bank_y1, &bank_y2, &bank_y3, &bank_y4,
			  &bank_y5, &bank_y6, &bank_y7 }},
	[GPIO_Z] = { 1, { &bank_z }}
};

static GpioRegs *exynos5420_get_regs(Exynos5420Gpio *gpio)
{
	GpioGroup *group = &groups[gpio->group];
	GpioBank *bank = group->banks[gpio->bank];
	return bank->regs;
}

static int exynos5420_gpio_use(Exynos5420Gpio *me, unsigned use)
{
	GpioRegs *regs = exynos5420_get_regs(me);

	uint32_t con = readl(&regs->con);
	con &= ~(0xf << (me->index * 4));
	con |= ((use & 0xf) << (me->index * 4));
	writel(con, &regs->con);

	return 0;
}

static int exynos5420_gpio_set_pud(Exynos5420Gpio *me, unsigned value)
{
	GpioRegs *regs = exynos5420_get_regs(me);

	uint32_t pud = readl(&regs->pud);
	pud &= ~(0x3 << (me->index * 2));
	pud |= ((value & 0x3) << (me->index * 2));
	writel(pud, &regs->pud);

	return 0;
}

static int exynos5420_gpio_get_value(GpioOps *me)
{
	assert(me);
	Exynos5420Gpio *gpio = container_of(me, Exynos5420Gpio, ops);
	GpioRegs *regs = exynos5420_get_regs(gpio);

	if (!gpio->dir_set) {
		if (gpio->use(gpio, 0))
			return -1;
		gpio->dir_set = 1;
	}

	return (readl(&regs->dat) >> gpio->index) & 0x1;
}

static int exynos5420_gpio_set_value(GpioOps *me, unsigned value)
{
	assert(me);
	Exynos5420Gpio *gpio = container_of(me, Exynos5420Gpio, ops);
	GpioRegs *regs = exynos5420_get_regs(gpio);

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

Exynos5420Gpio *new_exynos5420_gpio(unsigned group, unsigned bank,
				    unsigned index)
{
	die_if(group >= ARRAY_SIZE(groups) ||
	       bank >= groups[group].num_banks ||
	       groups[group].banks[bank] == NULL ||
	       index >= groups[group].banks[bank]->num_gpios,
	       "GPIO parameters (%d, %d, %d) out of bounds.\n",
	       group, bank, index);

	Exynos5420Gpio *gpio = xzalloc(sizeof(*gpio));
	gpio->use = &exynos5420_gpio_use;
	gpio->pud = &exynos5420_gpio_set_pud;
	gpio->group = group;
	gpio->bank = bank;
	gpio->index = index;
	return gpio;
}

Exynos5420Gpio *new_exynos5420_gpio_input(unsigned group, unsigned bank,
					  unsigned index)
{
	Exynos5420Gpio *gpio = new_exynos5420_gpio(group, bank, index);
	gpio->ops.get = &exynos5420_gpio_get_value;
	return gpio;
}

Exynos5420Gpio *new_exynos5420_gpio_output(unsigned group, unsigned bank,
					   unsigned index)
{
	Exynos5420Gpio *gpio = new_exynos5420_gpio(group, bank, index);
	gpio->ops.set = &exynos5420_gpio_set_value;
	return gpio;
}
