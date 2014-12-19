/*
 * This file is part of the depthcharge project.
 *
 * Copyright (C) 2014 Imagination Technologies
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <arch/cpu.h>
#include "base/init_funcs.h"
#include "drivers/flash/spi.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/bus/spi/imgtec_spfi.h"
#include "drivers/storage/mtd/mtd.h"
#include "drivers/storage/mtd/stream.h"
#include "drivers/storage/spi_gpt.h"
#include "drivers/storage/mtd/nand/spi_nand.h"
#include "drivers/gpio/gpio.h"
#include "vboot/util/flag.h"

#define IMG_SPIM0_BASE_ADDRESS	0xB8100F00
#define IMG_SPIM1_BASE_ADDRESS	0xB8101000


static int board_setup(void)
{
	struct imgtec_spi *spfi;
	MtdDevCtrlr *mtd;
	SpiGptCtrlr *virtual_dev;
	UsbHostController *usb_host;

	flag_install(FLAG_DEVSW, new_gpio_low());
	flag_install(FLAG_LIDSW, new_gpio_high());
	flag_install(FLAG_PHYS_PRESENCE, new_gpio_high());
	flag_install(FLAG_RECSW, new_gpio_high());
	flag_install(FLAG_PWRSW, new_gpio_low());
	flag_install(FLAG_WPSW, new_gpio_low());

	usb_host = new_usb_hc(DWC2, 0xB8120000);
	list_insert_after(&usb_host->list_node, &usb_host_controllers);

	spfi = new_imgtec_spi(IMG_SPIM1_BASE_ADDRESS, 0);
	flash_set_ops(&new_spi_flash(&spfi->ops)->ops);

	spfi = new_imgtec_spi(IMG_SPIM1_BASE_ADDRESS, 1);
	mtd = new_spi_nand(&spfi->ops);
	virtual_dev = new_spi_gpt("RW_GPT", new_mtd_stream(mtd), NULL);
	list_insert_after(&virtual_dev->block_ctrlr.list_node,
				&fixed_block_dev_controllers);
	return 0;
}

INIT_FUNC(board_setup);
