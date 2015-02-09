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
#include "drivers/storage/mtd/mtd.h"
#include "drivers/storage/mtd/stream.h"
#include "drivers/storage/spi_gpt.h"
#include "drivers/storage/mtd/nand/bcm_nand.h"
#include "drivers/bus/usb/usb.h"

static int board_setup(void)
{
	bcm_qspi *qspi = new_bcm_qspi(BCM_QSPI_BASE, SPI_MODE_3, 50000000);
	flash_set_ops(&new_spi_flash(&qspi->ops)->ops);

	MtdDevCtrlr *mtd = new_bcm_nand((void *)BCM_NAND_BASE,
					(void *)BCM_NAND_IDM_BASE,
					4 /* ONFI timing mode */);
	SpiGptCtrlr *virtual_dev = new_spi_gpt("RW_GPT",
					       new_mtd_stream(mtd),
					       "nand");
	list_insert_after(&virtual_dev->block_ctrlr.list_node,
			  &fixed_block_dev_controllers);

	UsbHostController *usb_host20 = new_usb_hc(EHCI, 0x18048000);
	UsbHostController *usb_host11 = new_usb_hc(OHCI, 0x18048800);
	list_insert_after(&usb_host20->list_node, &usb_host_controllers);
	list_insert_after(&usb_host11->list_node, &usb_host_controllers);

	return 0;
}

INIT_FUNC(board_setup);
