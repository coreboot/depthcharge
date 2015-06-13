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
#include "boot/fit.h"
#include "drivers/bus/i2c/imgtec_i2c.h"
#include "drivers/bus/spi/imgtec_spfi.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/flash/spi.h"
#include "drivers/gpio/gpio.h"
#include "drivers/gpio/imgtec_pistachio.h"
#include "drivers/storage/mtd/mtd.h"
#include "drivers/storage/mtd/nand/spi_nand.h"
#include "drivers/storage/mtd/stream.h"
#include "drivers/storage/spi_gpt.h"
#include "drivers/tpm/slb9635_i2c.h"
#include "drivers/tpm/tpm.h"
#include "vboot/callbacks/nvstorage_flash.h"
#include "vboot/util/flag.h"

#define SPIM_INTERFACE	1
#define NOR_CS		0
#define NOR_CS_GPIO	0
#define NAND_CS		1

/*
 * The following structure described unique hardware properties of all designs
 * using the concerto/urara board, as well as the compatible string to look
 * for in the FIT header. The board_id field is the key for searching in an
 * array of these structures.
 *
 * Board ID is determined by coreboot and passed through the sysinfo
 * structure.
 */
struct board_conf {
	uint8_t board_id;
	uint8_t i2c_interface;
	uint8_t nand_cs_gpio;
	const char *compatible;
} board_configs[] = {
	/*
	 * Each time a new board needs to be supported, an element in this
	 * array will have to be added. The board IDs must match the coreboot
	 * ones defined in src/mainboard/google/urara/urara_boardid.h
	 */
	{0, 0, 58, "img,pistachio-bub"},
	{1, 3,  1, "google,buranku"},
	{2, 3,  1, "google,derwent"},
	{3, 3,  1, "google,jaguar"},
	{4, 3,  1, "google,kennet"},
	{5, 3,  1, "google,space"},
};

static struct board_conf *pick_board_config(void)
{
	int i;
	static struct board_conf *board_config;

	if (!board_config) {
		board_config = board_configs;
		for (i = 0;
		     i < ARRAY_SIZE(board_configs);
		     i++, board_config++) {
			if (board_config->board_id == lib_sysinfo.board_id)
				return board_config;
		}

		printf("Invalid board ID %d, using 0\n", lib_sysinfo.board_id);
		board_config = board_configs;
	}

	return board_config;
}

static const DtPathMap mac_maps[] = {
	{ 1, "ethernet@18140000/mac-address" },
	{ 1, "wifi@18480000/mac-address-0" },
	{ 1, "wifi@18480000/mac-address-1" },
	{}
};

static const DtPathMap calibration_maps[] = {
	{ 1, "wifi@18480000/calibration-data", "wifi_calibration0" },
	{}
};

static int fix_device_tree(DeviceTreeFixup *fixup, DeviceTree *tree)
{
	return dt_set_mac_addresses(tree, mac_maps) |
		dt_set_wifi_calibration(tree, calibration_maps);
}

static DeviceTreeFixup urara_dt_fixup = {
	.fixup = fix_device_tree
};

static int board_setup(void)
{
	ImgSpi *spfi;
	MtdDevCtrlr *mtd;
	SpiGptCtrlr *virtual_dev;
	UsbHostController *usb_host;
	ImgGpio *img_gpio;
	ImgI2c *img_i2c;
	struct board_conf *conf = pick_board_config();

	flag_install(FLAG_DEVSW, new_gpio_low());
	flag_install(FLAG_LIDSW, new_gpio_high());
	flag_install(FLAG_PHYS_PRESENCE, new_gpio_high());
	flag_install(FLAG_RECSW, new_gpio_high());
	flag_install(FLAG_PWRSW, new_gpio_low());
	flag_install(FLAG_WPSW, new_gpio_low());

	usb_host = new_usb_hc(DWC2, 0xB8120000);
	list_insert_after(&usb_host->list_node, &usb_host_controllers);

	img_gpio = new_imgtec_gpio_output(NOR_CS_GPIO);
	spfi = new_imgtec_spi(SPIM_INTERFACE, NOR_CS, &(img_gpio->ops));
	flash_set_ops(&new_spi_flash(&spfi->ops)->ops);

	img_gpio = new_imgtec_gpio_output(conf->nand_cs_gpio);
	spfi = new_imgtec_spi(SPIM_INTERFACE, NAND_CS, &(img_gpio->ops));

	mtd = new_spi_nand(&spfi->ops);
	virtual_dev = new_spi_gpt("RW_GPT", new_mtd_stream(mtd),
				  "spi@18101000/flash@1");
	list_insert_after(&virtual_dev->block_ctrlr.list_node,
				&fixed_block_dev_controllers);
	img_i2c = new_imgtec_i2c(conf->i2c_interface, 100000, 33333333);
	tpm_set_ops(&new_slb9635_i2c(&(img_i2c->ops), 0x20)->base.ops);

	fit_set_compat(conf->compatible);

	list_insert_after(&urara_dt_fixup.list_node, &device_tree_fixups);

	return 0;
}

INIT_FUNC(board_setup);
