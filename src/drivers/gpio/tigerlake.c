// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2020 Google LLC
 * Copyright 2020 Intel Corporation.
 *
 * GPIO driver for Intel Tigerlake SOC.
 */

#include "drivers/gpio/tigerlake.h"
#include "drivers/soc/tigerlake.h"

/* This is ordered to match ACPI and OS driver. */
static struct gpio_community communities[] = {
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

const struct gpio_community *gpio_get_community(int pad)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(communities); i++) {
		struct gpio_community *c = &communities[i];

		if (pad >= c->min && pad <= c->max) {
                        c->pad_cfg.pcr_base_addr = PCH_PCR_BASE_ADDRESS;
                        c->pad_cfg.dw_offset = PAD_CFG_DW_OFFSET;
                        c->pad_cfg.hostsw_offset = HOSTSW_OWN_REG_OFFSET;
			return c;
		}
	}

	return NULL;
}

GpioOps *new_tigerlake_gpio_input_from_coreboot(uint32_t port)
{
	return &(new_platform_gpio(port)->ops);
}
