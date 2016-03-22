/*
 * Copyright 2012 Google Inc.  All rights reserved.
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

#ifndef __DRIVERS_BUS_I2S_I2S_H__
#define __DRIVERS_BUS_I2S_I2S_H__

#include <stdint.h>

typedef struct I2sOps
{
	int (*send)(struct I2sOps *me, uint32_t *data, unsigned int length);
} I2sOps;

#endif /* __DRIVERS_BUS_I2S_I2S_H__ */
