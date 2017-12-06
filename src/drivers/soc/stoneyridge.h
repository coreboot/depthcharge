
/*
 * Copyright (C) 2017 Google Inc.
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

#ifndef __DRIVERS_SOC_STONEYRIDGE_H__
#define __DRIVERS_SOC_STONEYRIDGE_H__

/* I2C definitions */

/* Designware Controller runs at 133MHz */
#define AP_I2C_CLK_MHZ  133

#define AP_I2C0_ADDR  0xFEDC2000
#define AP_I2C1_ADDR  0xFEDC3000
#define AP_I2C2_ADDR  0xFEDC4000
#define AP_I2C3_ADDR  0xFEDC5000

/* GPE definitions */
int stoneyridge_get_gpe(int gpe);

#define EVENT_STATUS	0xfed80200

#endif /* __DRIVERS_SOC_STONEYRIDGE_H__ */
