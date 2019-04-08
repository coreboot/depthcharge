/*
 * Copyright (C) 2016 Google Inc.
 * Copyright (C) 2016 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc.
 */

#include <arch/io.h>
#include <libpayload.h>
#include <stdint.h>
#include <config.h>

#include "drivers/gpio/gpio.h"
#include "drivers/gpio/apollolake.h"
#include "drivers/soc/apollolake.h"
#include "drivers/soc/common/pcr.h"

#if CONFIG(DRIVER_SOC_GLK)
static const struct gpio_community {
	int first_pad;
	int port_id;
} gpio_communities[] = {
	{
		.port_id = PCH_PCR_PID_GPIO_SCC,
		.first_pad = SCC_OFFSET,
	}, {
		.port_id = PCH_PCR_PID_GPIO_AUDIO,
		.first_pad = AUDIO_OFFSET,
	}, {
		.port_id = PCH_PCR_PID_GPIO_NORTH,
		.first_pad = N_OFFSET,
	}, {
		.port_id = PCH_PCR_PID_GPIO_NORTHWEST,
		.first_pad = NW_OFFSET,
	}
};
#else
static const struct gpio_community {
	int first_pad;
	int port_id;
} gpio_communities[] = {
	{
		.port_id = PCH_PCR_PID_GPIO_SOUTHWEST,
		.first_pad = SW_OFFSET,
	}, {
		.port_id = PCH_PCR_PID_GPIO_WEST,
		.first_pad = W_OFFSET,
	}, {
		.port_id = PCH_PCR_PID_GPIO_NORTHWEST,
		.first_pad = NW_OFFSET,
	}, {
		.port_id = PCH_PCR_PID_GPIO_NORTH,
		.first_pad = 0,
	}
};
#endif

static const struct gpio_community *gpio_get_community(int pad)
{
	const struct gpio_community *map = gpio_communities;

	while (map->first_pad > pad)
		map++;

	return map;
}

static void *gpio_dw_regs(int pad)
{
	const struct gpio_community *comm;
	uint8_t *regs;
	size_t pad_relative;

	comm = gpio_get_community(pad);
	if (comm == NULL)
		return NULL;

	regs = pcr_port_regs(PCH_PCR_BASE_ADDRESS, comm->port_id);

	pad_relative = pad - comm->first_pad;

	/* DW0 and Dw1 regs are 4 bytes each. */
	return &regs[PAD_CFG_DW_OFFSET + pad_relative * PAD_CFG_DW_OFFSET_MUL ];
}

static void *gpio_hostsw_reg(int pad, size_t *bit)
{
	const struct gpio_community *comm;
	uint8_t *regs;
	size_t pad_relative;

	comm = gpio_get_community(pad);
	if (comm == NULL)
		return NULL;

	regs = pcr_port_regs(PCH_PCR_BASE_ADDRESS, comm->port_id);

	pad_relative = pad - comm->first_pad;

	/* Update the bit for this pad. */
	*bit = (pad_relative % HOSTSW_OWN_PADS_PER);

	/* HostSw regs are 4 bytes each. */
	regs = &regs[HOSTSW_OWN_REG_OFFSET];
	return &regs[(pad_relative / HOSTSW_OWN_PADS_PER) * 4];
}

static void gpio_handle_pad_mode(const pad_config *cfg)
{
	size_t bit;
	uint32_t *hostsw_own_reg;
	uint32_t reg;

	bit = 0;
	hostsw_own_reg = gpio_hostsw_reg(cfg->pad, &bit);

	reg = read32(hostsw_own_reg);
	reg &= ~(1U << bit);

	if ((cfg->attrs & PAD_FIELD(HOSTSW, GPIO)) == PAD_FIELD(HOSTSW, GPIO))
		reg |= (HOSTSW_GPIO << bit);
	else
		reg |= (HOSTSW_ACPI << bit);

	write32(hostsw_own_reg, reg);
}

static void gpio_configure_pad(const pad_config *cfg)
{
 	uint32_t *dw_regs;
        uint32_t reg;
        uint32_t termination;
        uint32_t dw0;
        const uint32_t termination_mask = PAD_TERM_MASK << PAD_TERM_SHIFT;

        dw_regs = gpio_dw_regs(cfg->pad);
        if (dw_regs == NULL)
                return;

        dw0 = cfg->dw0;
        write32(&dw_regs[0], dw0);
        reg = read32(&dw_regs[1]);
        reg &= ~termination_mask;
        termination = cfg->attrs;
        termination &= termination_mask;
        reg |= termination;
        write32(&dw_regs[1], reg);

        gpio_handle_pad_mode(cfg);
}

static int __apollolake_gpio_get(int gpio_num)
{
	uint32_t *dw_regs;
	uint32_t reg;

	dw_regs = gpio_dw_regs(gpio_num);
	if (dw_regs == NULL)
		return -1;

	reg = read32(&dw_regs[0]);

	return (reg >> GPIORXSTATE_SHIFT) & GPIORXSTATE_MASK;
}

static void __apollolake_gpio_set(int gpio_num, int value)
{
	uint32_t *dw_regs;
	uint32_t reg;

	dw_regs = gpio_dw_regs(gpio_num);
	if (dw_regs == NULL)
		return;

	reg = read32(&dw_regs[0]);
	reg &= ~PAD_FIELD(GPIOTXSTATE, MASK);
	reg |= PAD_FIELD_VAL(GPIOTXSTATE, value);
	write32(&dw_regs[0], reg);
}

/*
 * Depthcharge GPIO interface.
 */

static int apollolake_gpio_configure(struct GpioCfg *gpio,
				     const pad_config *pad)
{
	gpio_configure_pad(pad);
	return 0;
}

static int apollolake_gpio_get(struct GpioOps *me)
{
	GpioCfg *gpio = container_of(me, GpioCfg, ops);
	return __apollolake_gpio_get(gpio->gpio_num);
}

static int apollolake_gpio_set(struct GpioOps *me, unsigned value)
{
	GpioCfg *gpio = container_of(me, GpioCfg, ops);
	__apollolake_gpio_set(gpio->gpio_num, value);
	return 0;
}

static int apollolake_gpio_get_fast(struct GpioOps *me)
{
	GpioCfg *gpio = container_of(me, GpioCfg, ops);
	return (gpio->current_dw0 >> GPIOTXSTATE_SHIFT) & GPIOTXSTATE_MASK;
}

static int apollolake_gpio_set_fast(struct GpioOps *me, unsigned value)
{
	GpioCfg *gpio = container_of(me, GpioCfg, ops);
	gpio->current_dw0 &= ~PAD_FIELD(GPIOTXSTATE, MASK);
	gpio->current_dw0 |= PAD_FIELD_VAL(GPIOTXSTATE, value);
	write32(&gpio->dw_regs[0], gpio->current_dw0);
	return 0;
}

GpioCfg *new_apollolake_gpio(int gpio_num)
{
	GpioCfg *gpio = xzalloc(sizeof(GpioCfg));

	gpio->ops.get   = &apollolake_gpio_get;
	gpio->ops.set   = &apollolake_gpio_set;
	gpio->configure = &apollolake_gpio_configure;
	gpio->gpio_num  = gpio_num;
	gpio->dw_regs   = gpio_dw_regs(gpio_num);

	return gpio;
}

static int apollolake_gpio_cleanup(CleanupFunc *cleanup, CleanupType type)
{
	GpioCfg *gpio = cleanup->data;
	write32(&gpio->dw_regs[0], gpio->save_dw0);
	write32(&gpio->dw_regs[1], gpio->save_dw1);
	return 0;
}

static void register_apollolake_gpio_cleanup(GpioCfg *gpio)
{
	/* Save initial GPIO state */
	gpio->save_dw0 = read32(&gpio->dw_regs[0]);
	gpio->save_dw1 = read32(&gpio->dw_regs[1]);

	/* Register callback to restore GPIO state on exit */
	gpio->cleanup.cleanup = &apollolake_gpio_cleanup;
	gpio->cleanup.types = CleanupOnHandoff | CleanupOnLegacy;
	gpio->cleanup.data = gpio;
	list_insert_after(&gpio->cleanup.list_node, &cleanup_funcs);
}

GpioCfg *new_apollolake_gpio_input(int gpio_num)
{
	GpioCfg *gpio = new_apollolake_gpio(gpio_num);

	/* Restore this GPIO to original state on exit */
	register_apollolake_gpio_cleanup(gpio);

	/* Configure the GPIO as a standard input pin */
	pad_config pad = PAD_CFG_GPI(gpio_num, NONE, DEEP);
	gpio_configure_pad(&pad);

	return gpio;
}

GpioCfg *new_apollolake_gpio_output(int gpio_num, unsigned value)
{
	GpioCfg *gpio = new_apollolake_gpio(gpio_num);

	/* Restore this GPIO to original state on exit */
	register_apollolake_gpio_cleanup(gpio);

	/* Configure the GPIO as a standard output pin */
	pad_config pad = PAD_CFG_GPO(gpio_num, value, DEEP);
	gpio_configure_pad(&pad);

	/* Store the current DW0 and DW1 register state */
	gpio->current_dw0 = read32(&gpio->dw_regs[0]);

	/* Use the fast methods that cache the register contents */
	gpio->ops.get = &apollolake_gpio_get_fast;
	gpio->ops.set = &apollolake_gpio_set_fast;

	return gpio;
}

GpioOps *new_apollolake_gpio_input_from_coreboot(uint32_t port)
{
	return &(new_apollolake_gpio(port)->ops);
}
