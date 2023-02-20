// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2020 Intel Corporation.
 *
 * GPIO driver for Intel Alder Lake SOC.
 */

#include <libpayload.h>

#include "drivers/gpio/alderlake.h"
#include "drivers/soc/alderlake.h"

/* This is ordered to match ACPI and OS driver. */
static struct gpio_community communities[] = {
	{	/* GPP B, T, A */
		.port_id = PCH_PCR_PID_GPIOCOM0,
		.min = GPIO_COM0_START,
		.max = GPIO_COM0_END,
	},
	{	/* GPP S, D, H for ADL-P/M
		   GPP S, I, D, H for ADL-N */
		.port_id = PCH_PCR_PID_GPIOCOM1,
		.min = GPIO_COM1_START,
		.max = GPIO_COM1_END,
	},
	{	/* GPD */
		.port_id = PCH_PCR_PID_GPIOCOM2,
		.min = GPIO_COM2_START,
		.max = GPIO_COM2_END,
	},
	{	/* GPP F, C, HVMOS, E */
		.port_id = PCH_PCR_PID_GPIOCOM4,
		.min = GPIO_COM4_START,
		.max = GPIO_COM4_END,
	},
	{	/* GPP R, SPI0 */
		.port_id = PCH_PCR_PID_GPIOCOM5,
		.min = GPIO_COM5_START,
		.max = GPIO_COM5_END,
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

GpioOps *new_alderlake_gpio_input_from_coreboot(uint32_t port)
{
	return &(new_platform_gpio(port)->ops);
}
