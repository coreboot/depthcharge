/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _MOCKS_FMAP_AREA_H
#define _MOCKS_FMAP_AREA_H

#include "image/fmap.h"
#include "drivers/flash/flash.h"

extern FmapArea *mock_area;
extern void *mock_flash_buf;

/* Setup global mock state for flash related operations */
void set_mock_fmap_area(FmapArea *area, void *mirror_buf);

enum {
	MOCK_FLASH_SUCCESS = 0,
	MOCK_FLASH_FAIL,
};

#endif /* _MOCKS_FMAP_AREA_H */
