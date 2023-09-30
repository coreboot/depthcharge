/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __DRIVERS_FLASH_MEMMAPPED_H__
#define __DRIVERS_FLASH_MEMMAPPED_H__

#include <stdint.h>

#include "drivers/flash/flash.h"

typedef struct {
	FlashOps ops;
} MmapFlash;

MmapFlash *new_mmap_flash(void);

#endif /* __DRIVERS_FLASH_MEMMAPPED_H__ */
