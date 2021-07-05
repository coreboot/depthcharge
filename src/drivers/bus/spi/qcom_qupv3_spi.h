/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2018-2019, The Linux Foundation.  All rights reserved.
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

#ifndef __DRIVERS_BUS_SPI_QUP_H__
#define __DRIVERS_BUS_SPI_QUP_H__

#include "drivers/bus/spi/spi.h"
#include "drivers/soc/qcom_qup_se.h"

typedef struct {
	SpiOps ops;
	QupRegs *reg_addr;
} QupSpi;

QupSpi *new_qup_spi(uintptr_t reg_addr);

#endif /* __DRIVERS_BUS_SPI_QUP_H__ */

