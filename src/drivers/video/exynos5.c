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

enum {
	FIMD_VIDCON0_ENVID = 2,
	FIMD_VIDCON0_ENVID_F = 1
};

int disable_fimd_dma(struct CleanupFunc *cleanup, CleanupType type)
{
	// The vidcon0 register is at the start of the register block.
	uint32_t *fimd_vidcon0 = (uint32_t *)cleanup->data;

	uint32_t vidcon0 = readl(fimd_vidcon0);
	vidcon0 &= ~(FIMD_VIDCON0_ENVID | FIMD_VIDCON0_ENVID_F);
	writel(vidcon0, fimd_vidcon0);

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
