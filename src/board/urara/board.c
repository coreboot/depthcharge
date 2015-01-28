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
#include "drivers/gpio/imgtec_pistachio.h"
#include "boot/fit.h"

#define IMG_SPIM0_BASE_ADDRESS	0xB8100F00
#define IMG_SPIM1_BASE_ADDRESS	0xB8101000

#define IMG_SPIM1_BASE_ADDRESS_CS0	0
#define IMG_SPIM1_BASE_ADDRESS_CS1	58
#define IMG_SPIM1_BASE_ADDRESS_CS2	31
#define IMG_SPIM1_BASE_ADDRESS_CS3	56
#define IMG_SPIM1_BASE_ADDRESS_CS4	57

#define IMG_SPIM0_BASE_ADDRESS_CS0	2
#define IMG_SPIM0_BASE_ADDRESS_CS1	1
#define IMG_SPIM0_BASE_ADDRESS_CS2	55
#define IMG_SPIM0_BASE_ADDRESS_CS3	56
#define IMG_SPIM0_BASE_ADDRESS_CS4	57

#define SPIM_MFIO(base,cs)	base##_CS##cs

static int board_setup(void)
{
	ImgSpi *spfi;
	MtdDevCtrlr *mtd;
	SpiGptCtrlr *virtual_dev;
	UsbHostController *usb_host;
	ImgGpio *img_gpio;

	flag_install(FLAG_DEVSW, new_gpio_low());
	flag_install(FLAG_LIDSW, new_gpio_high());
	flag_install(FLAG_PHYS_PRESENCE, new_gpio_high());
	flag_install(FLAG_RECSW, new_gpio_high());
	flag_install(FLAG_PWRSW, new_gpio_low());
	flag_install(FLAG_WPSW, new_gpio_low());

	usb_host = new_usb_hc(DWC2, 0xB8120000);
	list_insert_after(&usb_host->list_node, &usb_host_controllers);

	img_gpio = new_imgtec_gpio_output(SPIM_MFIO(IMG_SPIM1_BASE_ADDRESS, 0));
	spfi = new_imgtec_spi(IMG_SPIM1_BASE_ADDRESS, 0, &(img_gpio->ops));
	flash_set_ops(&new_spi_flash(&spfi->ops)->ops);

	img_gpio = new_imgtec_gpio_output(SPIM_MFIO(IMG_SPIM1_BASE_ADDRESS, 1));
	spfi = new_imgtec_spi(IMG_SPIM1_BASE_ADDRESS, 1, &(img_gpio->ops));

	mtd = new_spi_nand(&spfi->ops);
	virtual_dev = new_spi_gpt("RW_GPT", new_mtd_stream(mtd), NULL);
	list_insert_after(&virtual_dev->block_ctrlr.list_node,
				&fixed_block_dev_controllers);
	fit_set_compat("img,pistachio-bub");
	return 0;
}

INIT_FUNC(board_setup);
