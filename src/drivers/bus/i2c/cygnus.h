/*
 * Copyright 2015 Google Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __DRIVERS_BUS_I2C_CYGNUS_H__
#define __DRIVERS_BUS_I2C_CYGNUS_H__

#include "i2c.h"

typedef struct CygnusI2c {
	I2cOps ops;
	void *reg_addr; // CygnusI2cReg*
} CygnusI2c;

CygnusI2c *new_cygnus_i2c(uintptr_t regs, unsigned int hz);

#endif				/* __DRIVERS_BUS_I2C_CYGNUS_H__ */
