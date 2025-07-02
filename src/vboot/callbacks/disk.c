/*
 * Copyright 2012 Google LLC
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
 */

#include <assert.h>
#include <libpayload.h>
#include <vb2_api.h>
#include <vboot_api.h>

#include "base/timestamp.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/stream.h"

vb2_error_t VbExDiskRead(vb2ex_disk_handle_t handle, uint64_t lba_start,
			 uint64_t lba_count, void *buffer)
{
	BlockDevOps *ops = &((BlockDev *)handle)->ops;
	if (ops->read(ops, lba_start, lba_count, buffer) != lba_count) {
		printf("Read failed.\n");
		return VB2_ERROR_UNKNOWN;
	}
	return VB2_SUCCESS;
}

vb2_error_t VbExDiskWrite(vb2ex_disk_handle_t handle, uint64_t lba_start,
			  uint64_t lba_count, const void *buffer)
{
	BlockDevOps *ops = &((BlockDev *)handle)->ops;
	if (ops->write(ops, lba_start, lba_count, buffer) != lba_count) {
		printf("Write failed.\n");
		return VB2_ERROR_UNKNOWN;
	}
	return VB2_SUCCESS;
}

vb2_error_t VbExStreamOpen(vb2ex_disk_handle_t handle, uint64_t lba_start,
			   uint64_t lba_count, VbExStream_t *stream_ptr)
{
	BlockDevOps *ops = &((BlockDev *)handle)->ops;
	*stream_ptr = (VbExStream_t)ops->new_stream(ops, lba_start, lba_count);
	if (*stream_ptr == NULL) {
		printf("Stream open failed.\n");
		return VB2_ERROR_UNKNOWN;
	}
	return VB2_SUCCESS;
}

vb2_error_t VbExStreamSkip(VbExStream_t stream, uint32_t bytes)
{
	StreamOps *dev = (StreamOps *)stream;

	if (!dev->skip) {
		printf("Stream skip function not implemented.\n");
		return VB2_ERROR_UNKNOWN;
	}

	if (dev->skip(dev, bytes) != bytes) {
		printf("Stream skip failed.\n");
		return VB2_ERROR_UNKNOWN;
	}

	return VB2_SUCCESS;
}

vb2_error_t VbExStreamRead(VbExStream_t stream, uint32_t bytes, void *buffer)
{
	StreamOps *dev = (StreamOps *)stream;
	int ret = dev->read(dev, bytes, buffer);
	if (ret != bytes) {
		printf("Stream read failed.\n");
		return VB2_ERROR_UNKNOWN;
	}

	// Vboot first reads some headers from the front of the kernel partition
	// and then the whole kernel body in one call. We assume that any read
	// larger than 1MB is the kernel body, and thus the last read.
	if (bytes > MiB)
		timestamp_add_now(TS_VB_READ_KERNEL_DONE);

	return VB2_SUCCESS;
}

void VbExStreamClose(VbExStream_t stream)
{
	StreamOps *dev = (StreamOps *)stream;
	dev->close(dev);
}
