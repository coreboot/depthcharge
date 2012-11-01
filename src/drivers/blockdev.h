/*
 * Copyright (c) 2012 The Chromium OS Authors.
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

#ifndef __DRIVERS_BLOCKDEV_H__
#define __DRIVERS_BLOCKDEV_H__

#include <stdint.h>

typedef uint64_t lba_t;

typedef struct BlockDev {
	const char *name;
	int removable;
	unsigned block_size;
	lba_t block_count;
	lba_t (*read)(struct BlockDev *dev, lba_t start, lba_t count,
		      void *buffer);
	lba_t (*write)(struct BlockDev *dev, lba_t start, lba_t count,
		       const void *buffer);
	void *dev_data;

	struct BlockDev *next;
} BlockDev;

#endif /* __DRIVERS_BLOCKDEV_H__ */
