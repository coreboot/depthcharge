
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

#ifndef __DRIVERS_SOC_PICASSO_H__
#define __DRIVERS_SOC_PICASSO_H__

#include "drivers/storage/mmc.h"

#define EMMCHC 0xFEDD5000

void emmc_set_ios(MmcCtrlr *mmc_ctrlr);
int emmc_supports_hs400(void);

/* I2C definitions */

/* Designware Controller runs at 133MHz */
#define AP_I2C_CLK_MHZ  133

#define AP_I2C2_ADDR  0xFEDC4000
#define AP_I2C3_ADDR  0xFEDC5000
#define AP_I2C4_ADDR  0xFEDC6000

#endif /* __DRIVERS_SOC_PICASSO_H__ */
