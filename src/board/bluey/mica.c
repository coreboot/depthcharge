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
#include "drivers/soc/x1p42100.h"
#include "variant.h"

#define NVME_PCI_DEV PCI_DEV(0x1, 0, 0)

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

#define SLAVE_ID_PWM 0x00
#define GPIO5_DIG_OUT_SOURCE_CTL ((SLAVE_ID_PWM << 16) | 0xBC44)
#define GPIO_OUTPUT_LOW 0x00

void variant_display_teardown(QcomSpmi *pmic_spmi)
{
	printf("Tearing down the display for Mica\n");
	/* Disable backlight pwm */
	pmic_spmi->write8(pmic_spmi, GPIO5_DIG_OUT_SOURCE_CTL, GPIO_OUTPUT_LOW);

	/* Disable power on */
	GpioOps *display_vdd = sysinfo_lookup_gpio("Panel VDD en", 1,
				new_gpio_output_from_coreboot);
	gpio_set(display_vdd, 0);

	/* Disable vtsp */
	GpioOps *display_vtsp = sysinfo_lookup_gpio("Panel VTSP en", 1,
				new_gpio_output_from_coreboot);
	gpio_set(display_vtsp, 0);
}
