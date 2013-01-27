/*
 * Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

#include <libpayload.h>

#include "drivers/i2c/i2c.h"

int i2c_read(uint8_t bus, uint8_t chip, uint32_t addr, int addr_len,
	     uint8_t *data, int data_len)
{
	return 0;
}
int i2c_write(uint8_t bus, uint8_t chip, uint32_t addr, int addr_len,
	      uint8_t *data, int data_len)
{
	return 0;
}
