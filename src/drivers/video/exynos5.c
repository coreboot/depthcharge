/*
 * Copyright 2013 Google Inc.
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

#include <arch/io.h>
#include <stdint.h>

#include "base/cleanup_funcs.h"
#include "base/init_funcs.h"
#include "config.h"

// An incomplete FIMD register map.
typedef struct Exynos5Fimd {
	uint32_t vidcon0;
	uint8_t _1[0x1c];
	uint32_t wincon0;
	uint32_t wincon1;
	uint32_t wincon2;
	uint32_t wincon3;
	uint32_t wincon4;
	uint32_t shadowcon;
	uint8_t _2[0x8];
	uint32_t vidosd0a;
	uint32_t vidosd0b;
	uint32_t vidosd0c;
	uint8_t _3[0x54];
	uint32_t vidw00add0b0;
	uint8_t _4[0x2c];
	uint32_t vidw00add1b0;
	uint8_t _5[0x2c];
	uint32_t vidw00add2;
	uint8_t _6[0x3c];
	uint32_t w1keycon0;
	uint32_t w1keycon1;
	uint32_t w2keycon0;
	uint32_t w2keycon1;
	uint32_t w3keycon0;
	uint32_t w3keycon1;
	uint32_t w4keycon0;
	uint32_t w4keycon1;
	uint8_t _7[0x20];
	uint32_t win0map;
	uint8_t _8[0xdc];
	uint32_t blendcon;
	uint8_t _9[0x18];
	uint32_t dpclkcon;
} Exynos5Fimd;

enum {
	Channel0En = 1 << 0
};

int disable_fimd_dma(struct CleanupFunc *cleanup, CleanupType type)
{
	Exynos5Fimd *fimd = (Exynos5Fimd *)cleanup->data;

	writel(0, &fimd->wincon0);
	uint32_t shadowcon = readl(&fimd->shadowcon);
	writel(shadowcon & ~Channel0En, &fimd->shadowcon);

	return 0;
}

CleanupFunc dma_cleanup = {
	&disable_fimd_dma,
	CleanupOnHandoff,
	(void *)(uintptr_t)(CONFIG_DRIVER_VIDEO_EXYNOS5_BASE)
};

int install_dma_cleanup(void)
{
	list_insert_after(&dma_cleanup.list_node, &cleanup_funcs);
	return 0;
}

INIT_FUNC(install_dma_cleanup);
