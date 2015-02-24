/*
 * Copyright 2015 Google Inc.
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

#include "base/init_funcs.h"
#include "drivers/flash/spi.h"
#include "drivers/bus/spi/bcm_qspi.h"

static int board_setup(void)
{
	bcm_qspi *qspi = new_bcm_qspi(BCM_QSPI_BASE, SPI_MODE_3, 50000000);
	flash_set_ops(&new_spi_flash(&qspi->ops)->ops);
	return 0;
}

INIT_FUNC(board_setup);
