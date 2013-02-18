/*
 * Copyright 2013 Google Inc. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

#include <libpayload.h>

#include "drivers/bus/i2c/i2c.h"

int i2c_read(uint8_t bus, uint8_t chip, uint32_t addr, int addr_len,
	     uint8_t *data, int data_len)
{
	printf("%s not implemented.\n", __func__);
	return 1;
}
int i2c_write(uint8_t bus, uint8_t chip, uint32_t addr, int addr_len,
	      uint8_t *data, int data_len)
{
	printf("%s not implemented.\n", __func__);
	return 1;
}
