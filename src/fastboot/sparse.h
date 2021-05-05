/*
 * Copyright 2021 Google Inc.
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

#ifndef __FASTBOOT_SPARSE_H__
#define __FASTBOOT_SPARSE_H__

#include "fastboot/fastboot.h"
#include "fastboot/disk.h"

#if 0
#define FB_TRACE_SPARSE FB_DEBUG
#else
#define FB_TRACE_SPARSE(...)
#endif

int is_sparse_image(void *image_addr);
int write_sparse_image(fastboot_session_t *fb, struct fastboot_disk *disk,
		       GptEntry *partition, void *image_addr,
		       uint64_t image_size);

#endif // __FASTBOOT_SPARSE_H__
