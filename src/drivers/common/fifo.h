/*
 * Copyright 2013 Google Inc.  All rights reserved.
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

#ifndef __DRIVERS_COMMON_FIFO_H__
#define __DRIVERS_COMMON_FIFO_H__

#include <stdint.h>

typedef struct TxFifoOps {

	// Sends arbitrary bytes into FIFO.
	// Returns the actual bytes sent, or -1 on error.
	ssize_t (*send)(struct TxFifoOps *me, const void *buf, size_t len);

	// Returns 1 if the FIFO is full, otherwise 0.
	int (*is_full)(struct TxFifoOps *me);

	// (Optional) Returns the total capacity of FIFO in bytes.
	size_t (*capacity)(struct TxFifoOps *me);

	// (Optional) Returns the current size of data already in FIFO.
	size_t (*size)(struct TxFifoOps *me);

} TxFifoOps;

typedef struct RxFifoOps {

	// Receives arbitrary bytes from FIFO.
	// Returns the actual bytes received, or -1 on error.
	ssize_t (*recv)(struct RxFifoOps *me, void *buf, size_t len);

	// Returns 1 if the FIFO is empty, otherwise 0.
	int (*is_empty)(struct RxFifoOps *me);

	// (Optional) Returns the total capacity of FIFO in bytes.
	size_t (*capacity)(struct RxFifoOps *me);

	// (Optional) Returns the current size of data available in FIFO.
	size_t (*size)(struct RxFifoOps *me);

} RxFifoOps;

#endif /* __DRIVERS_COMMON_FIFO_H__ */
