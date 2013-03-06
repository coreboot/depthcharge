/*
 * This file is part of the depthcharge project.
 *
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA, 02110-1301 USA
 */

#include <stdint.h>

#ifndef __DRIVERS_BUS_I2S_I2S_H__
#define __DRIVERS_BUS_I2S_I2S_H__

// Sends the given data through i2s.
int i2s_send(uint32_t *data, unsigned int length);

// Initialise i2s transmiter.
int i2s_transfer_init(int lr_frame_size, int bits_per_sample,
		      int bit_frame_size);

#endif /* __DRIVERS_BUS_I2S_I2S_H__ */
