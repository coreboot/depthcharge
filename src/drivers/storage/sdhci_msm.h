/*
 * Copyright (C) 2019, The Linux Foundation.  All rights reserved.
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

#ifndef __DRIVERS_STORAGE_SDHCI_MSM_H__
#define __DRIVERS_STORAGE_SDHCI_MSM_H__

#include "drivers/storage/sdhci.h"
#include "drivers/gpio/gpio.h"

/* SDM specific defines */

#define XO_CLK		(19.2*MHz)

/* SDHC specific defines */
#define SDCC_HC_REG_DLL_CONFIG		0x200
#define DLL_CFG_POR_VAL			0x6000642C
#define DLL_EN				(1 << 16)
#define CLK_OUT_EN			(1 << 18)
#define DLL_PDN				(1 << 29)
#define DLL_RST				(1 << 30)

#define SDCC_HC_REG_DLL_STATUS		0x208
#define DDR_DLL_LOCK_JDR		(1 << 11)

#define SDCC_HC_VENDOR_SPECIFIC_FUNC1	0x20C
#define VENDOR_SPEC_FUN1_POR_VAL	0xA0C
#define HC_MCLK_SEL_HS400		(3 << 8)
#define HC_MCLK_SEL_MASK		(3 << 8)
#define HC_IO_PAD_PWR_SWITCH_EN		(1 << 15)
#define HC_IO_PAD_PWR_SWITCH		(1 << 16)
#define HC_SELECT_IN_EN			(1 << 18)
#define HC_SELECT_IN_MASK		(7 << 19)
#define HC_SELECT_IN_HS400		(6 << 19)
#define SDCC_HC_VENDOR_SPECIFIC_FUNC3	0x250
#define VENDOR_SPEC_FUN3_POR_VAL	0x02226040
#define SDCC_HC_VENDOR_SPECIFIC_CAPABILITIES0	0x21C

#define SDCC_HC_REG_DLL_CONFIG_2	0x254
#define DLL_CFG2_POR_VAL		0x0020A000
#define DDR_CAL_EN			(1 << 0)
#define MCLK_FREQ_CALC_SHIFT		10
#define MCLK_FREQ_CALC_MASK		(0xFF << 10)
#define FLL_CYCLE_CNT			(1 << 18)
#define DLL_CLOCK_DISABLE		(1 << 21)

#define SDCC_HC_REG_DDR_CONFIG		0x25C
#define DDR_CONFIG_POR_VAL		0x80040868

#define SDCC_HC_VENDOR_SPECIFIC_DDR200_CFG	0x224
#define CMDIN_RCLK_EN		(1 << 1)

SdhciHost *new_sdhci_msm_host(uintptr_t ioaddr, unsigned int platform_info,
			      int clock_max, GpioOps *cd_gpio);

#endif /* __DRIVERS_STORAGE_SDHCI_MSM_H__ */
