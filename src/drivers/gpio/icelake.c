/*
 * Copyright (C) 2018 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <arch/io.h>
#include <libpayload.h>
#include <stdint.h>

#include "base/cleanup_funcs.h"
#include "base/container_of.h"
#include "drivers/gpio/icelake.h"
#include "drivers/soc/common/pcr.h"
#include "drivers/soc/icelake.h"

/* There are 4 communities with 8 GPIO groups (GPP_[A:G] and GPD) */
struct gpio_community {
	int port_id;
	/* Inclusive pads within the community. */
	int min;
	int max;
};

/* This is ordered to match ACPI and OS driver. */
static const struct gpio_community communities[] = {
	{
		.port_id = PCH_PCR_PID_GPIOCOM0,
		.min = GPP_G0,
		.max = GPP_A23,
	}, /* GPP_G,GPP_B, GPP_A */
	{
		.port_id = PCH_PCR_PID_GPIOCOM1,
		.min = GPP_H0,
		.max = GPP_F19,
	}, /* GPP_H, GPP_D, GPP_F */
	{
		.port_id = PCH_PCR_PID_GPIOCOM2,
		.min = GPD0,
		.max = GPD11,
	}, /* GPD */
	{
		.port_id = PCH_PCR_PID_GPIOCOM4,
		.min = GPP_C0,
		.max = GPP_E23,
	}, /* GPP_C, GPP_E */
	{
		.port_id = PCH_PCR_PID_GPIOCOM5,
		.min = GPP_R0,
		.max = GPP_S7,
	}, /* GPP R, S */
};

static inline size_t gpios_in_community(const struct gpio_community *comm)
{
	/* max is inclusive */
	return comm->max - comm->min + 1;
}

static inline size_t groups_in_community(const struct gpio_community *comm)
{
       size_t n;
       switch (comm->port_id) {
       case PCH_PCR_PID_GPIOCOM0:
		n = 3;
		break;
	case PCH_PCR_PID_GPIOCOM1:
		n = 3;
		break;
	case PCH_PCR_PID_GPIOCOM2:
		n = 1;
		break;
	case PCH_PCR_PID_GPIOCOM4:
		n = 2;
		break;
	case PCH_PCR_PID_GPIOCOM5:
		n = 2;
		break;
	default:
		n = 0;
       }
       return n;
}

static inline int gpio_index_gpd(int gpio)
{
	if (gpio >= GPD0 && gpio <= GPD11)
		return 1;
	return 0;
}

static const struct gpio_community *gpio_get_community(int pad)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(communities); i++) {
		const struct gpio_community *c = &communities[i];

		if (pad >= c->min && pad <= c->max)
			return c;
	}

	return NULL;
}

static void *gpio_dw_regs(int pad)
{
	const struct gpio_community *comm;
	uint16_t dw_config_base = PAD_CFG_DW_OFFSET;
	uint8_t *regs;
	size_t pad_relative;

	comm = gpio_get_community(pad);

	if (comm == NULL)
		return NULL;

	regs = pcr_port_regs(PCH_PCR_BASE_ADDRESS, comm->port_id);

	pad_relative = pad - comm->min;

	if (comm->port_id == PCH_PCR_PID_GPIOCOM0) {
		if (pad > GPP_B23 && pad <= GPP_A23)
			/* Skip GSPI0_CLK_LOOPBK and GSPI1_CLK_LOOPBK */
			dw_config_base += (2 * GPIO_DWx_WIDTH(3));
	}

	if (comm->port_id == PCH_PCR_PID_GPIOCOM1) {
		if (pad > GPP_D19 && pad <= GPP_F19)
			/* Skip GSPI2_CLK_LOOPB */
			dw_config_base += (1 * GPIO_DWx_WIDTH(3));
	}

	if (comm->port_id == PCH_PCR_PID_GPIOCOM4) {
		if (pad > GPP_C23 && pad <= GPP_E23)
			dw_config_base += (6 * GPIO_DWx_WIDTH(3));
	}
	/* DW0, DW1, DW2 and DW3 regs are 4 bytes each. */
	return &regs[dw_config_base + pad_relative * GPIO_DWx_WIDTH(3)];
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

	pad_relative = pad - comm->min;

	/* Update the bit for this pad. */
	*bit = (pad_relative % HOSTSW_OWN_PADS_PER);

	/* HostSw regs are 4 bytes each. */
	regs = &regs[HOSTSW_OWN_REG_OFFSET];
	return &regs[(pad_relative / HOSTSW_OWN_PADS_PER) * 4];
}

static void gpio_handle_pad_mode(const struct pad_config *cfg)
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

static void gpio_configure_pad(const struct pad_config *cfg)
{
	uint32_t *dw_regs;
	uint32_t reg;
	uint32_t termination;
	uint32_t dw0;
	uint32_t dw2;
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

	dw2 = cfg->dw2;
	write32(&dw_regs[2], dw2);

	gpio_handle_pad_mode(cfg);
}

static void gpio_configure_pads(const struct pad_config *cfg, size_t num_pads)
{
	size_t i;

	for (i = 0; i < num_pads; i++)
		gpio_configure_pad(cfg + i);
}

static int __icelake_gpio_get(int gpio_num)
{
	uint32_t *dw_regs;
	uint32_t reg;

	dw_regs = gpio_dw_regs(gpio_num);

	if (dw_regs == NULL)
		return -1;

	reg = read32(&dw_regs[0]);

	return (reg >> GPIORXSTATE_SHIFT) & GPIORXSTATE_MASK;
}

static void __icelake_gpio_set(int gpio_num, int value)
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
	/* GPIO port ids support posted write semantics. */
}

/*
 * Depthcharge GPIO interface.
 */
static int icelake_gpio_configure(struct GpioCfg *gpio,
				  const struct pad_config *pad,
				  size_t num_pads)
{
	gpio_configure_pads(pad, num_pads);
	return 0;
}

static int icelake_gpio_get(struct GpioOps *me)
{
	GpioCfg *gpio = container_of(me, GpioCfg, ops);
	return __icelake_gpio_get(gpio->gpio_num);
}

static int icelake_gpio_set(struct GpioOps *me, unsigned value)
{
	GpioCfg *gpio = container_of(me, GpioCfg, ops);
	__icelake_gpio_set(gpio->gpio_num, value);
	return 0;
}

static int icelake_gpio_get_fast(struct GpioOps *me)
{
	GpioCfg *gpio = container_of(me, GpioCfg, ops);
	return (gpio->current_dw0 >> GPIORXSTATE_SHIFT) & GPIORXSTATE_MASK;
}

static int icelake_gpio_set_fast(struct GpioOps *me, unsigned value)
{
	GpioCfg *gpio = container_of(me, GpioCfg, ops);
	gpio->current_dw0 &= ~PAD_FIELD(GPIOTXSTATE, MASK);
	gpio->current_dw0 |= PAD_FIELD_VAL(GPIOTXSTATE, value);
	write32(&gpio->dw_regs[0], gpio->current_dw0);
	return 0;
}

GpioCfg *new_icelake_gpio(int gpio_num)
{
	GpioCfg *gpio = xzalloc(sizeof(GpioCfg));

	gpio->ops.get   = &icelake_gpio_get;
	gpio->ops.set   = &icelake_gpio_set;
	gpio->configure = &icelake_gpio_configure;
	gpio->gpio_num  = gpio_num;
	gpio->dw_regs   = gpio_dw_regs(gpio_num);

	return gpio;
}

static int icelake_gpio_cleanup(CleanupFunc *cleanup, CleanupType type)
{
	GpioCfg *gpio = cleanup->data;
	write32(&gpio->dw_regs[0], gpio->save_dw0);
	write32(&gpio->dw_regs[1], gpio->save_dw1);
	write32(&gpio->dw_regs[2], gpio->save_dw2);
	return 0;
}

static void register_icelake_gpio_cleanup(GpioCfg *gpio)
{
	/* Save initial GPIO state */
	gpio->save_dw0 = read32(&gpio->dw_regs[0]);
	gpio->save_dw1 = read32(&gpio->dw_regs[1]);
	gpio->save_dw2 = read32(&gpio->dw_regs[2]);

	/* Register callback to restore GPIO state on exit */
	gpio->cleanup.cleanup = &icelake_gpio_cleanup;
	gpio->cleanup.types = CleanupOnHandoff | CleanupOnLegacy;
	gpio->cleanup.data = gpio;
	list_insert_after(&gpio->cleanup.list_node, &cleanup_funcs);
}

GpioCfg *new_icelake_gpio_input(int gpio_num)
{
	GpioCfg *gpio = new_icelake_gpio(gpio_num);

	/* Restore this GPIO to original state on exit */
	register_icelake_gpio_cleanup(gpio);

	/* Configure the GPIO as a standard input pin */
	struct pad_config pad = PAD_CFG_GPI(gpio_num, NONE, DEEP);
	gpio_configure_pad(&pad);

	return gpio;
}

GpioCfg *new_icelake_gpio_output(int gpio_num, unsigned value)
{
	GpioCfg *gpio = new_icelake_gpio(gpio_num);

	/* Restore this GPIO to original state on exit */
	register_icelake_gpio_cleanup(gpio);

	/* Configure the GPIO as a standard output pin */
	struct pad_config pad = PAD_CFG_GPO(gpio_num, value, DEEP, NONE,
								DISABLE);
	gpio_configure_pad(&pad);

	/* Store the current DW0 register state */
	gpio->current_dw0 = read32(&gpio->dw_regs[0]);

	/* Use the fast methods that cache the register contents */
	gpio->ops.get = &icelake_gpio_get_fast;
	gpio->ops.set = &icelake_gpio_set_fast;

	return gpio;
}

GpioOps *new_icelake_gpio_input_from_coreboot(uint32_t port)
{
	return &(new_icelake_gpio(port)->ops);
}
