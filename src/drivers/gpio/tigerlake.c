// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2020 Google LLC
 * Copyright 2020 Intel Corporation.
 *
 * GPIO driver for Intel Tigerlake SOC.
 */

#include <arch/io.h>
#include <libpayload.h>
#include <stdint.h>

#include "base/cleanup_funcs.h"
#include "base/container_of.h"
#include "drivers/gpio/tigerlake.h"
#include "drivers/soc/common/pcr.h"
#include "drivers/soc/tigerlake.h"

/* There are 5 communities with 15 GPIO groups */
struct gpio_community {
	int port_id;
	/* Inclusive pads within the community. */
	int min;
	int max;
};

/* This is ordered to match ACPI and OS driver. */
static const struct gpio_community communities[] = {
	{	/* GPP B, T, A */
		.port_id = PCH_PCR_PID_GPIOCOM0,
		.min = GPP_B0,
		.max = GPP_A24,
	},
	{	/* GPP S, H, D, U, VGPIO */
		.port_id = PCH_PCR_PID_GPIOCOM1,
		.min = GPP_S0,
		.max = vI2S2_RXD,
	},
	{	/* GPD */
		.port_id = PCH_PCR_PID_GPIOCOM2,
		.min = GPD0,
		.max = GPD_DRAM_RESETB,
	},
	{	/* GPP C, F, HVCMOS, E, JTAG */
		.port_id = PCH_PCR_PID_GPIOCOM4,
		.min = GPP_C0,
		.max = GPP_DBG_PMODE,
	},
	{	/* GPP R, SPI */
		.port_id = PCH_PCR_PID_GPIOCOM5,
		.min = GPP_R0,
		.max = GPP_CLK_LOOPBK,
	},
};

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
	uint8_t *regs;
	size_t pad_relative;

	comm = gpio_get_community(pad);

	if (comm == NULL)
		return NULL;

	regs = pcr_port_regs(PCH_PCR_BASE_ADDRESS, comm->port_id);

	pad_relative = pad - comm->min;

	/* DW0, DW1, DW2 and DW3 regs are 4 bytes each. */
	return &regs[PAD_CFG_DW_OFFSET + pad_relative * GPIO_DWx_WIDTH(3)];
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

	if (!hostsw_own_reg)
		return;

	reg = read32(hostsw_own_reg);
	reg &= ~(1U << bit);

	if ((cfg->attrs & PAD_FIELD(HOSTSW, GPIO)) ==
		PAD_FIELD(HOSTSW, GPIO))
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

static int __tigerlake_gpio_get(int gpio_num)
{
	uint32_t *dw_regs;
	uint32_t reg;

	dw_regs = gpio_dw_regs(gpio_num);

	if (dw_regs == NULL)
		return -1;

	reg = read32(&dw_regs[0]);

	return (reg >> GPIORXSTATE_SHIFT) & GPIORXSTATE_MASK;
}

static void __tigerlake_gpio_set(int gpio_num, int value)
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

static int tigerlake_gpio_configure(struct GpioCfg *gpio,
				  const struct pad_config *pad,
				  size_t num_pads)
{
	gpio_configure_pads(pad, num_pads);
	return 0;
}

static int tigerlake_gpio_get(struct GpioOps *me)
{
	GpioCfg *gpio = container_of(me, GpioCfg, ops);
	return __tigerlake_gpio_get(gpio->gpio_num);
}

static int tigerlake_gpio_set(struct GpioOps *me, unsigned int value)
{
	GpioCfg *gpio = container_of(me, GpioCfg, ops);
	__tigerlake_gpio_set(gpio->gpio_num, value);
	return 0;
}

static int tigerlake_gpio_get_fast(struct GpioOps *me)
{
	GpioCfg *gpio = container_of(me, GpioCfg, ops);
	return (gpio->current_dw0 >> GPIORXSTATE_SHIFT) & GPIORXSTATE_MASK;
}

static int tigerlake_gpio_set_fast(struct GpioOps *me, unsigned int value)
{
	GpioCfg *gpio = container_of(me, GpioCfg, ops);
	gpio->current_dw0 &= ~PAD_FIELD(GPIOTXSTATE, MASK);
	gpio->current_dw0 |= PAD_FIELD_VAL(GPIOTXSTATE, value);
	write32(&gpio->dw_regs[0], gpio->current_dw0);
	return 0;
}

GpioCfg *new_tigerlake_gpio(int gpio_num)
{
	GpioCfg *gpio = xzalloc(sizeof(GpioCfg));

	gpio->ops.get   = &tigerlake_gpio_get;
	gpio->ops.set   = &tigerlake_gpio_set;
	gpio->configure = &tigerlake_gpio_configure;
	gpio->gpio_num  = gpio_num;
	gpio->dw_regs   = gpio_dw_regs(gpio_num);

	return gpio;
}

static int tigerlake_gpio_cleanup(CleanupFunc *cleanup, CleanupType type)
{
	GpioCfg *gpio = cleanup->data;
	write32(&gpio->dw_regs[0], gpio->save_dw0);
	write32(&gpio->dw_regs[1], gpio->save_dw1);
	write32(&gpio->dw_regs[2], gpio->save_dw2);
	return 0;
}

static void register_tigerlake_gpio_cleanup(GpioCfg *gpio)
{
	/* Save initial GPIO state */
	gpio->save_dw0 = read32(&gpio->dw_regs[0]);
	gpio->save_dw1 = read32(&gpio->dw_regs[1]);
	gpio->save_dw2 = read32(&gpio->dw_regs[2]);

	/* Register callback to restore GPIO state on exit */
	gpio->cleanup.cleanup = &tigerlake_gpio_cleanup;
	gpio->cleanup.types = CleanupOnHandoff | CleanupOnLegacy;
	gpio->cleanup.data = gpio;
	list_insert_after(&gpio->cleanup.list_node, &cleanup_funcs);
}

GpioCfg *new_tigerlake_gpio_input(int gpio_num)
{
	GpioCfg *gpio = new_tigerlake_gpio(gpio_num);

	/* Restore this GPIO to original state on exit */
	register_tigerlake_gpio_cleanup(gpio);

	/* Configure the GPIO as a standard input pin */
	struct pad_config pad = PAD_CFG_GPI(gpio_num, NONE, DEEP);
	gpio_configure_pad(&pad);

	return gpio;
}

GpioCfg *new_tigerlake_gpio_output(int gpio_num, unsigned int value)
{
	GpioCfg *gpio = new_tigerlake_gpio(gpio_num);

	/* Restore this GPIO to original state on exit */
	register_tigerlake_gpio_cleanup(gpio);

	/* Configure the GPIO as a standard output pin */
	struct pad_config pad = PAD_CFG_GPO(gpio_num, value,
					    DEEP, NONE, DISABLE);
	gpio_configure_pad(&pad);

	/* Store the current DW0 register state */
	gpio->current_dw0 = read32(&gpio->dw_regs[0]);

	/* Use the fast methods that cache the register contents */
	gpio->ops.get = &tigerlake_gpio_get_fast;
	gpio->ops.set = &tigerlake_gpio_set_fast;

	return gpio;
}

GpioOps *new_tigerlake_gpio_input_from_coreboot(uint32_t port)
{
	return &(new_tigerlake_gpio(port)->ops);
}
