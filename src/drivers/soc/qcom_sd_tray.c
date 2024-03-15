/*
 * This file is part of the coreboot project.
 *
 * Copyright (c) 2021-2022 Qualcomm Technologies
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
#include "drivers/gpio/sysinfo.h"
#include "drivers/gpio/qcom_gpio.h"
#include "qcom_sd_tray.h"
#include "qcom_spmi.h"

typedef struct {
	GpioOps ops;
	QcomSpmi *spmi;
	uintptr_t offset;
	u32 val;
	GpioOps *underlying;
	int prev;
} QcomSdTray;

int new_qcom_sd_tray_gpio_get(GpioOps *sd_cd)
{
	QcomSdTray *op = container_of(sd_cd, QcomSdTray, ops);

	int val = gpio_get(op->underlying);
	if (val != op->prev) {
		if (val) {
			/*
			 * We used to be disconnected but now have an SD card.
			 * Need to force the regulator on.
			 */
			op->spmi->write8(op->spmi, op->offset, op->val);
			/*
			 * give it some ramp-up time to stabilize
			 */
			mdelay(10);
		}
		op->prev = val;
	}
	return val;
}

GpioOps *new_qcom_sd_tray_cd_wrapper(GpioOps *sd_cd, QcomSpmi *spmi,
					uintptr_t offset, u32 val)
{
	QcomSdTray *op = xzalloc(sizeof(*op));
	op->spmi = spmi;
	op->offset = offset;
	op->val = val;
	op->underlying = sd_cd;

	op->ops.get = &new_qcom_sd_tray_gpio_get;

	return &op->ops;
}
