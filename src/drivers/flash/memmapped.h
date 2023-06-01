/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __DRIVERS_FLASH_MEMMAPPED_H__
#define __DRIVERS_FLASH_MEMMAPPED_H__

#include <stdint.h>

#include "drivers/flash/flash.h"

typedef struct {
	FlashOps ops;
	FlashOps *base_ops;
} MmapFlash;

/*
 * Create a mmap-backed flash with a base FlashOps `base_ops`.
 *
 * It will perform mmap read whenever possible. Otherwise, for write/erase
 * operations or out-of-bound read operations, `base_ops` will be used.
 */
MmapFlash *new_mmap_backed_flash(FlashOps *base_ops);

static inline MmapFlash *new_mmap_flash(void)
{
	return new_mmap_backed_flash(NULL);
}

#endif /* __DRIVERS_FLASH_MEMMAPPED_H__ */
