/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2026, The Linux Foundation.  All rights reserved.
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

#define MDP_INTF_2_TIMING_ENGINE_ADDR	0x0AE37000
#define DSI1_CTRL_ADDR			0x0AE96004
#define DSI1_SW_RESET_ADDR		0x0AE96118

#define PMIC_A_SLAVE_ID			0x00
#define PMIC_D_SLAVE_ID			0x03
#define BL_EN_DIG_OUT_SOURCE_CTL	((PMIC_D_SLAVE_ID << 16) | 0x8B44)
#define BL_PWM_DIG_OUT_SOURCE_CTL	((PMIC_A_SLAVE_ID << 16) | 0xBC44)
#define GPIO_OUTPUT_HIGH		0x80
#define GPIO_OUTPUT_LOW			0x00

#define GPIO_PANEL_RESET		GPIO(26)
#define GPIO_TS_RESET_L			GPIO(48)
#define GPIO_PANEL_AUX_POWER_ON		GPIO(114)
#define GPIO_PANEL_BIAS_EN		GPIO(217)

uintptr_t variant_get_ec_spi_base(void)
{
	return QUP_SERIAL13_BASE;
}

uintptr_t variant_get_gsc_i2c_base(void)
{
	return QUP_SERIAL10_BASE;
}

bool variant_nvme_supported(void)
{
	return false;
}

bool variant_ufs_supported(void)
{
	return true;
}

void variant_display_teardown(void)
{
	QcomSpmi *pmic_spmi = new_qcom_spmi(PMIC_CORE_REGISTERS_ADDR,
				PMIC_REG_CHAN0_ADDR,
				PMIC_REG_LAST_CHAN_ADDR - PMIC_REG_FIRST_CHAN_ADDR);
	int value = pmic_spmi->read8(pmic_spmi, BL_EN_DIG_OUT_SOURCE_CTL);
	if (value == GPIO_OUTPUT_HIGH) {
		printf("Tearing down the display for Meru\n");

		/* Disable backlight enable and PWM */
		pmic_spmi->write8(pmic_spmi, BL_EN_DIG_OUT_SOURCE_CTL, GPIO_OUTPUT_LOW);
		pmic_spmi->write8(pmic_spmi, BL_PWM_DIG_OUT_SOURCE_CTL, GPIO_OUTPUT_LOW);

		/* Disable INTF_2 timing engine */
		write32p(MDP_INTF_2_TIMING_ENGINE_ADDR, 0x0);

		/* Wait for a vsync pulse */
		mdelay(20);

		/* Reset DSI1 controller */
		write32p(DSI1_CTRL_ADDR, 0x0);
		write32p(DSI1_SW_RESET_ADDR, 0x1);
		mdelay(20);
		write32p(DSI1_SW_RESET_ADDR, 0x0);

		/* Assert panel reset and touchscreen reset */
		mdelay(5);
		gpio_set(&new_gpio_output(GPIO_PANEL_RESET)->ops, 0);
		mdelay(5);
		gpio_set(&new_gpio_output(GPIO_TS_RESET_L)->ops, 0);

		/* Disable panel bias supply */
		gpio_set(&new_gpio_output(GPIO_PANEL_BIAS_EN)->ops, 0);
		mdelay(5);

		/* Disable auxiliary panel power */
		gpio_set(&new_gpio_output(GPIO_PANEL_AUX_POWER_ON)->ops, 0);

		/* Disable panel power */
		GpioOps *display_vdd = sysinfo_lookup_gpio("Panel VDD en", 1,
					new_gpio_output_from_coreboot);
		if (display_vdd)
			gpio_set(display_vdd, 0);

		/* Disable touchscreen power */
		GpioOps *display_vtsp = sysinfo_lookup_gpio("Panel VTSP en", 1,
					new_gpio_output_from_coreboot);
		if (display_vtsp)
			gpio_set(display_vtsp, 0);
	}
	free(pmic_spmi);
}
