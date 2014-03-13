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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA, 02110-1301 USA
 */

#ifndef __BASE_TIMESTAMP_H__
#define __BASE_TIMESTAMP_H__

#include <stdint.h>

enum timestamp_id {
	// Depthcharge entry IDs start at 1000.
	TS_START = 1000,

	TS_RO_PARAMS_INIT = 1001,
	TS_RO_VB_INIT = 1002,
	TS_RO_VB_SELECT_FIRMWARE = 1003,
	TS_RO_VB_SELECT_AND_LOAD_KERNEL = 1004,

	TS_RW_VB_SELECT_AND_LOAD_KERNEL = 1010,

	TS_VB_SELECT_AND_LOAD_KERNEL = 1020,

	TS_CROSSYSTEM_DATA = 1100,
	TS_START_KERNEL = 1101
};

void timestamp_init(void);
void timestamp_add(enum timestamp_id id, uint64_t ts_time);
void timestamp_add_now(enum timestamp_id id);

#endif /* __BASE_TIMESTAMP_H__ */
