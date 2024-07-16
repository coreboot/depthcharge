// SPDX-License-Identifier: GPL-2.0
/*
 * GPIO driver for Intel Meteor Lake SOC.
 */

#include <libpayload.h>

#include "drivers/gpio/pantherlake.h"
#include "drivers/soc/pantherlake.h"

/* This is ordered to match ACPI and OS driver. */
static struct gpio_community communities[] = {

	{	/* GPP V, C */
		.port_id = PCH_PCR_PID_GPIOCOM0,
		.min = COM0_GRP_PAD_START,
		.max = COM0_GRP_PAD_END,
	},
	{	/* GPP F, E */
		.port_id = PCH_PCR_PID_GPIOCOM1,
		.min = COM1_GRP_PAD_START,
		.max = COM1_GRP_PAD_END,
	},
	{	/* GPP JTAG, H, SPI0, VGPIO3 */
		.port_id = PCH_PCR_PID_GPIOCOM3,
		.min = COM3_GRP_PAD_START,
		.max = COM3_GRP_PAD_END,
	},
	{	/* GPP S */
		.port_id = PCH_PCR_PID_GPIOCOM4,
		.min = COM4_GRP_PAD_START,
		.max = COM4_GRP_PAD_END,
	},
	{	/* GPP B, D, VGPIO0 */
		.port_id = PCH_PCR_PID_GPIOCOM5,
		.min = COM5_GRP_PAD_START,
		.max = COM5_GRP_PAD_END,
	},
};

const struct gpio_community *gpio_get_community(int pad)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(communities); i++) {
		struct gpio_community *c = &communities[i];

		if (pad >= c->min && pad <= c->max) {
			c->pad_cfg.pcr_base_addr = (uintptr_t)PCH_PCR_BASE_ADDRESS;
			c->pad_cfg.dw_offset = PAD_CFG_DW_OFFSET;
			c->pad_cfg.hostsw_offset = HOSTSW_OWN_REG_OFFSET;
			return c;
		}
	}

	return NULL;
}

GpioOps *new_pantherlake_gpio_input_from_coreboot(uint32_t port)
{
        return &(new_platform_gpio(port)->ops);
}
