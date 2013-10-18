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

#include <libpayload.h>

#include "base/init_funcs.h"
#include "drivers/bus/spi/tegra.h"
#include "drivers/dma/tegra_apb.h"
#include "drivers/flash/spi.h"
#include "drivers/gpio/sysinfo.h"

static int board_setup(void)
{
	if (sysinfo_install_flags())
		return 1;

	void *dma_channel_bases[32];
	for (int i = 0; i < ARRAY_SIZE(dma_channel_bases); i++)
		dma_channel_bases[i] = (void *)(0x60021000 + 0x40 * i);

	TegraApbDmaController *dma_controller =
		new_tegra_apb_dma((void *)0x60020000, dma_channel_bases,
				  ARRAY_SIZE(dma_channel_bases));
	if (!dma_controller)
		return 1;

	TegraSpi *spi4 = new_tegra_spi(0x7000da00, dma_controller);
	if (!spi4)
		return 1;

	SpiFlash *flash = new_spi_flash(&spi4->ops, 0x400000);
	if (!flash || flash_set_ops(&flash->ops))
		return 1;

	return 0;
}

INIT_FUNC(board_setup);
