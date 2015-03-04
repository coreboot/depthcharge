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

#ifndef __BOARD_RUSH_RYU_FASTBOOT_H__
#define __BOARD_RUSH_RYU_FASTBOOT_H__

#include "config.h"
#include "drivers/storage/blockdev.h"

typedef enum {
	MMC_BDEV,
	FLASH_BDEV,
	BDEV_COUNT,
}bdev_t;

#if CONFIG_FASTBOOT_MODE

#include "fastboot/backend.h"
#include "fastboot/fastboot.h"
#include "fastboot/udc.h"

void fill_fb_info(BlockDevCtrlr *bdev_ctrlr_arr[BDEV_COUNT]);

#else

static inline void fill_fb_info(BlockDevCtrlr *bdev_ctrlr_arr[BDEV_COUNT]) {}

#endif /* CONFIG_FASTBOOT_MODE */

#endif /* __BOARD_RUSH_RYU_FASTBOOT_H__ */
