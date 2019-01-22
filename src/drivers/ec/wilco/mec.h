/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2019 Google LLC
 *
 * Microchip EC interface for Wilco Embedded Controller.
 */

#ifndef __DRIVERS_EC_WILCO_MEC_H__
#define __DRIVERS_EC_WILCO_MEC_H__

#include <stdint.h>

#include "drivers/ec/wilco/ec.h"

typedef enum MecIoType {
	MEC_IO_READ,
	MEC_IO_WRITE
} MecIoType;

/*
 * mec_io_bytes - Read / write bytes to MEC EMI port
 *
 * @type:    Indicate read or write operation
 * @base:    Base address for MEC EMI region
 * @offset:  Base read / write address
 * @buf:     Destination / source buffer
 * @size:    Number of bytes to read / write
 *
 * @returns 8-bit checksum of all bytes read or written
 */
uint8_t mec_io_bytes(MecIoType type, uint16_t base, uint16_t offset,
		     uint8_t *buf, size_t size);

#endif
