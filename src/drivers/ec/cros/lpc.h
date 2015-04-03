/*
 * Chromium OS EC driver - I2C interface
 *
 * Copyright 2013 Google Inc.
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __DRIVERS_EC_CROS_LPC_H__
#define __DRIVERS_EC_CROS_LPC_H__

#include "drivers/ec/cros/message.h"
#include "drivers/ec/cros/ec.h"

typedef struct
{
	CrosEcBusOps ops;
} CrosEcLpcBus;

typedef enum
{
	/* LPC access to command / data / memmap buffers */
	CROS_EC_LPC_BUS_GENERIC,
	/* Access memmap range through EMI unit */
	CROS_EC_LPC_BUS_MEC,
} CrosEcLpcBusVariant;

CrosEcLpcBus *new_cros_ec_lpc_bus(CrosEcLpcBusVariant variant);

#endif /* __DRIVERS_EC_CROS_LPC_H__ */
