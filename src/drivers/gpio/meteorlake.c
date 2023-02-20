// SPDX-License-Identifier: GPL-2.0
/*
 * GPIO driver for Intel Meteor Lake SOC.
 */

#include <libpayload.h>

#include "drivers/gpio/meteorlake.h"
#include "drivers/soc/meteorlake.h"

/* This is ordered to match ACPI and OS driver. */
static struct gpio_community communities[] = {

	{	/* GPP CPU, V, C */
		.port_id = PCH_PCR_PID_GPIOCOM0,
		.min = GPIO_COM0_START,
		.max = GPIO_COM0_END,
	},
	{	/* GPP A, E */
		.port_id = PCH_PCR_PID_GPIOCOM1,
		.min = GPIO_COM1_START,
		.max = GPIO_COM1_END,
	},
	{	/* GPP H, F, SPI0, VGPIO3 */
		.port_id = PCH_PCR_PID_GPIOCOM3,
		.min = GPIO_COM3_START,
		.max = GPIO_COM3_END,
	},
	{	/* GPP S, JTAG */
		.port_id = PCH_PCR_PID_GPIOCOM4,
		.min = GPIO_COM4_START,
		.max = GPIO_COM4_END,
	},
	{	/* GPP B, D, VGPIO0 */
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

GpioOps *new_meteorlake_gpio_input_from_coreboot(uint32_t port)
{
        return &(new_platform_gpio(port)->ops);
}
