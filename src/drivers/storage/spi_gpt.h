/*
 * Copyright 2014 Google Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __DRIVERS_STORAGE_MTD_SPI_GPT_H__
#define __DRIVERS_STORAGE_MTD_SPI_GPT_H__

#include <stdint.h>

#include "drivers/storage/blockdev.h"
#include "drivers/storage/stream.h"
#include "base/device_tree.h"
#include "image/fmap.h"

typedef struct {
	BlockDev block_dev;
	FmapArea area;
	struct SpiGptCtrlr *ctrlr;
} SpiGptDev;

typedef struct SpiGptCtrlr {
	BlockDevCtrlr block_ctrlr;
	const char *fmap_region;
	SpiGptDev *dev;
	StreamCtrlr *stream_ctrlr;
	const char *dt_path;
	DeviceTreeFixup fixup;
} SpiGptCtrlr;

/*
 * Create a block device based on read/write commands coming from a SPI
 * region (fmap_region, e.g., "rw_gpt") and a stream (e.g., a NAND device).
 * The address spaces may overlap and users choose which device to address
 * by the command (read/write vs new_stream).
 * Optionally, a 'dt_path' string addressing a device tree node can be
 * provided to install the partition table in that node, for communication
 * to the kernel.
 */
SpiGptCtrlr *new_spi_gpt(const char *fmap_region, StreamCtrlr *stream_ctrlr,
			 const char *dt_path);

#endif /* __DRIVERS_STORAGE_MTD_SPI_GPT_H__ */
