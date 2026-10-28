/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2025, The Linux Foundation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <libpayload.h>
#include "drivers/gpio/gpio.h"
#include "drivers/gpio/qcom_gpio.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/soc/qcom_spmi.h"
#include "drivers/soc/qcom_spmi_defs.h"
#include "drivers/soc/x1p42100.h"
#include "variant.h"

#define NVME_PCI_DEV PCI_DEV(0x1, 0, 0)

#define MDP_INTF_1_TIMING_ENGINE_ADDR	0x0AE36000
#define DSI0_CTRL_ADDR			0x0AE94004
#define DSI0_SW_RESET_ADDR		0x0AE94114

#define SLAVE_ID			0x03
#define GPIO4_DIG_OUT_SOURCE_CTL	((SLAVE_ID << 16) | 0x8B44)
#define GPIO_OUTPUT_HIGH		0x80
#define GPIO_OUTPUT_LOW			0x00

#define GPIO_PANEL_RESET		GPIO(26)

pcidev_t variant_get_nvme_pcidev()
{
	return NVME_PCI_DEV;
}

uintptr_t variant_get_ec_spi_base()
{
	return QUP_SERIAL13_BASE;
}

uintptr_t variant_get_gsc_i2c_base()
{
	return QUP_SERIAL10_BASE;
}

void variant_display_teardown(void)
{
	QcomSpmi *pmic_spmi = new_qcom_spmi(PMIC_CORE_REGISTERS_ADDR,
				PMIC_REG_CHAN0_ADDR,
				PMIC_REG_LAST_CHAN_ADDR - PMIC_REG_FIRST_CHAN_ADDR);
	uint8_t value = pmic_spmi->read8(pmic_spmi, GPIO4_DIG_OUT_SOURCE_CTL);
	if (value == GPIO_OUTPUT_HIGH) {
		printf("Tearing down the display for Pic\n");

		/* Disable backlight / panel rail */
		pmic_spmi->write8(pmic_spmi, GPIO4_DIG_OUT_SOURCE_CTL, GPIO_OUTPUT_LOW);

		/* Disable INTF_1 timing engine */
		write32p(MDP_INTF_1_TIMING_ENGINE_ADDR, 0x0);

		/* Wait for a vsync pulse */
		mdelay(20);

		/* Reset DSI0 controller */
		write32p(DSI0_CTRL_ADDR, 0x0);
		write32p(DSI0_SW_RESET_ADDR, 0x1);
		mdelay(20);
		write32p(DSI0_SW_RESET_ADDR, 0x0);

		/* Assert panel reset */
		mdelay(5);
		gpio_set(&new_gpio_output(GPIO_PANEL_RESET)->ops, 0);
		mdelay(5);

		/* Disable panel power */
		GpioOps *display_vdd = sysinfo_lookup_gpio("Panel VDD en", 1,
					new_gpio_output_from_coreboot);
		gpio_set(display_vdd, 0);

		/* Disable touchscreen power */
		GpioOps *display_vtsp = sysinfo_lookup_gpio("Panel VTSP en", 1,
					new_gpio_output_from_coreboot);
		gpio_set(display_vtsp, 0);
	}
	free(pmic_spmi);
}
