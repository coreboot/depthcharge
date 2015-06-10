/*
 * Copyright 2015 Google Inc.
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

#ifndef __FASTBOOT_BACKEND_H__
#define __FASTBOOT_BACKEND_H__

/* Headers from vboot for GPT manipulation */
#include <gpt.h>
#include <gpt_misc.h>

#include "drivers/storage/blockdev.h"
#include "drivers/storage/stream.h"

typedef enum {
	BE_SUCCESS,
	BE_PART_NOT_FOUND,
	BE_BDEV_NOT_FOUND,
	BE_IMAGE_SIZE_MULTIPLE_ERR,
	BE_IMAGE_OVERFLOW_ERR,
	BE_WRITE_ERR,
	BE_SPARSE_HDR_ERR,
	BE_CHUNK_HDR_ERR,
	BE_GPT_ERR,
	BE_NOT_HANDLED,
} backend_ret_t;

struct bdev_info {
	/* Name of block device */
	const char *name;
	/* Pointer to BlockDevCtrlr structure */
	BlockDevCtrlr *bdev_ctrlr;
	/* Pointer to BlockDev structure */
	BlockDev *bdev;
};

struct part_info {
	/* Name of partition */
	const char *part_name;
	/* Filesystem type of partition */
	const char *part_fs_type;
	/* Pointer to bdev_info structure */
	struct bdev_info *bdev_info;
	/* Boolean - Is the partition GPT dependent? 1-yes, 0-no */
	int gpt_based;
	/* Union for MMC v/s Flash properties */
	union {
		struct {
			Guid guid;
			int instance;
		};
		struct {
			/* Starting LBA */
			size_t base;
			/* Size in LBA */
			size_t size;
		};
	};
};

extern size_t fb_bdev_count;
extern struct bdev_info fb_bdev_list[];
extern size_t fb_part_count;
extern struct part_info fb_part_list[];

backend_ret_t board_write_partition(const char *name, void *image_addr,
				    size_t image_size);
backend_ret_t backend_erase_partition(const char *name);
backend_ret_t backend_write_partition(const char *name, void *image_addr,
				      size_t image_size);
uint64_t backend_get_part_size_bytes(const char *name);
const char *backend_get_part_fs_type(const char *name);
uint64_t backend_get_bdev_size_bytes(const char *name);
uint64_t backend_get_bdev_size_blocks(const char *name);
int is_sparse_image(void *image_addr);
struct part_info *get_part_info(const char *name);

static inline int fb_fill_bdev_list(int index, BlockDevCtrlr *bdev_ctrlr)
{
	if (index >= fb_bdev_count)
		return -1;

	fb_bdev_list[index].bdev_ctrlr = bdev_ctrlr;
	return 0;
}

int fb_fill_part_list(const char *name, size_t base, size_t size);

#define PART_GPT(part_name, part_fs, bdev_name, g, inst)		\
	{part_name, part_fs, bdev_name, 1, .guid = g, .instance = inst}
#define PART_NONGPT(part_name, part_fs, bdev_name, start, len)		\
	{part_name, part_fs, bdev_name, 0, .base = start, .size = len}

#define BDEV_ENTRY(bdev)	(&fb_bdev_list[(bdev)])
#define GPT_TYPE(type)		GPT_ENT_TYPE_##type

#endif /* __FASTBOOT_BACKEND_H__ */
