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
 */
#include <libpayload.h>
#include "tegra132.h"

static void * const winbuf_t_start_addr = (void *)(uintptr_t)0x54202000;
static void * const win_t_win_options = (void *)(uintptr_t)0x54201c00;

int tegra132_display_init(DisplayOps *me)
{
	uintptr_t phys_addr = lib_sysinfo.framebuffer.physical_address;

	/* Set the framebuffer address and enable the T window. */
	write32(winbuf_t_start_addr, phys_addr);
	write32(win_t_win_options, read32(win_t_win_options) | WIN_ENABLE);

	return 0;
}

int tegra132_display_stop(DisplayOps *me)
{
	/* Disable the T Window. */
	write32(win_t_win_options, read32(win_t_win_options) & ~WIN_ENABLE);
	return 0;
}

