/*
 * Copyright 2021 Google LLC
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

#ifndef __BASE_SPARSE_H__
#define __BASE_SPARSE_H__

#include "base/gpt.h"
#include "drivers/storage/blockdev.h"

#if 0
#define TRACE_SPARSE printf
#else
#define TRACE_SPARSE(...)
#endif

int is_sparse_image(void *image_addr);
enum gpt_io_ret write_sparse_image(BlockDev *disk, uint64_t part_start_lba,
				   uint64_t part_size_lba, void *image_addr,
				   uint64_t image_size);

#endif // __BASE_SPARSE_H__
