/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __DRIVERS_STORAGE_SLICE_H__
#define __DRIVERS_STORAGE_SLICE_H__

#include "drivers/storage/blockdev.h"

BlockDev *new_blockdev_slice(BlockDev *parent, lba_t offset, lba_t size);

#endif /* __DRIVERS_STORAGE_SLICE_H__ */
