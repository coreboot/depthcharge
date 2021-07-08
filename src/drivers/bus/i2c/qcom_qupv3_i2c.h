/*
 * This file is part of the depthcharge project.
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

#ifndef __DRIVERS_BUS_I2C_QUP_H__
#define __DRIVERS_BUS_I2C_QUP_H__

#include "drivers/bus/i2c/i2c.h"
#include "drivers/soc/qcom_qup_se.h"

typedef struct {
	I2cOps ops;
	QupRegs *reg_addr;
} QupI2c;

QupI2c *new_Qup_i2c(uintptr_t regs);

#endif /* __DRIVERS_BUS_I2C_QUP_H__ */
