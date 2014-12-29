/*
 * Copyright 2014 Google Inc.
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

#include <config.h>
#include <libpayload.h>
#include <sysinfo.h>
#include <stdio.h>

#include "base/init_funcs.h"
#include "boot/fit.h"
#include "boot/ramoops.h"
#include "drivers/bus/i2c/ipq806x_gsbi.h"
#include "drivers/bus/i2c/ipq806x.h"
#include "drivers/bus/spi/ipq806x.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/gpio/gpio.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/tpm/slb9635_i2c.h"
#include "drivers/tpm/tpm.h"
#include "vboot/callbacks/nvstorage_flash.h"
#include "vboot/util/flag.h"
#include "drivers/gpio/ipq806x.h"
#include "drivers/storage/ipq806x_mmc.h"

#include "board.h"

#define GPIO_SDCC_FUNC_VAL      2

#define MSM_SDC1_BASE		0x12400000

/* Structure describing properties of various Storm based boards. */
struct board_descriptor {
	const char *compat_string; // Match the device tree in FIT image.
	int calibration_needed;	   // Some boards need to populate WiFi
				   // calibration data.
};

static struct board_descriptor bdescriptor;

static void fill_board_descriptor(void)
{
	switch(lib_sysinfo.board_id) {
	case 2: /* Whirlwind SP3 */
		bdescriptor.compat_string = "google,whirlwind-sp3";
		bdescriptor.calibration_needed = 1;
		break;
	default:
		bdescriptor.compat_string = "google,storm-proto0";
		bdescriptor.calibration_needed = 0;
		break;
	}
}


/*
 * MAC address fixup. There might be more addresses in lib_sysinfo than
 * required. Just two need to be set, at the particular paths in the device
 * tree listed in the array below.
 */
static int set_mac_addresses(DeviceTree *tree)
{
	/*
	 * Map MAC addresses found in the coreboot table into the device tree
	 * locations.
	 * Some locations need to be forced to create, some locations are
	 * skipped if not present in the existing device tree.
	 */
	static const struct mac_addr_map {
		char *dt_path;
		int force_create;
	} maps[] = {
		{ "soc/ethernet@37000000", 0 },
		{ "soc/ethernet@37400000", 0 },
		{ "chosen/bluetooth", 1 }
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(maps); i++) {
		DeviceTreeNode *gmac_node;
		const struct mac_addr_map *map;

		if (i >= lib_sysinfo.num_macs)
			break;

		map = maps + i;
		gmac_node = dt_find_node_by_path(tree->root, map->dt_path,
						 NULL, NULL,
						 map->force_create);
		if (!gmac_node) {
			printf("Failed to %s %s in the device tree\n",
			       map->force_create ? "create" : "find",
			       map->dt_path);
			continue;
		}
		dt_add_bin_prop(gmac_node, "local-mac-address",
				lib_sysinfo.macs[i].mac_addr,
				sizeof(lib_sysinfo.macs[i].mac_addr));
	}
	return 0;
}

static int fix_device_tree(DeviceTreeFixup *fixup, DeviceTree *tree)
{
	int rv;

	rv = set_mac_addresses(tree);

	if (bdescriptor.calibration_needed)
		rv |= set_wifi_calibration(tree);

	return rv;
}

static DeviceTreeFixup ipq_enet_fixup = {
	.fixup = fix_device_tree
};

/* MMC bus GPIO assignments. */
enum storm_emmc_gpio {
	SDC1_DATA7 = 38,
	SDC1_DATA6 = 39,
	SDC1_DATA3 = 40,
	SDC1_DATA2 = 41,
	SDC1_CLK = 42,
	SDC1_DATA1 = 43,
	SDC1_DATA0 = 44,
	SDC1_CMD = 45,
	SDC1_DATA5 = 46,
	SDC1_DATA4 = 47,
};

/* Storm GPIO access wrapper. */
typedef struct
{
	GpioOps gpio_ops;	/* Depthcharge GPIO API wrapper. */
	struct cb_gpio *cbgpio;	/* GPIO description. */

} StormGpio;

static int get_gpio(struct GpioOps *me)
{
	unsigned value;
	StormGpio *gpio = container_of(me, StormGpio, gpio_ops);

	value = gpio_get_in_value(gpio->cbgpio->port);
	if (gpio->cbgpio->polarity == CB_GPIO_ACTIVE_LOW)
		value = !value;

	return value;
}

static StormGpio phys_presence_flag = {
	.gpio_ops = { .get = get_gpio }
};

static void install_phys_presence_flag(void)
{
	phys_presence_flag.cbgpio = sysinfo_lookup_gpio("recovery");

	if (!phys_presence_flag.cbgpio) {
		printf("%s failed retrieving recovery GPIO\n", __func__);
		return;
	}

	flag_install(FLAG_PHYS_PRESENCE, &phys_presence_flag.gpio_ops);
}

void board_mmc_gpio_config(void)
{
	unsigned i;
	unsigned char gpio_config_arr[] = {
		SDC1_DATA7, SDC1_DATA6, SDC1_DATA3,
		SDC1_DATA2, SDC1_DATA1, SDC1_DATA0,
		SDC1_CMD, SDC1_DATA5, SDC1_DATA4};

	gpio_tlmm_config_set(SDC1_CLK, GPIO_SDCC_FUNC_VAL,
		GPIO_PULL_DOWN, GPIO_16MA, 1);

	for (i = 0; i < ARRAY_SIZE(gpio_config_arr); i++) {
		gpio_tlmm_config_set(gpio_config_arr[i],
		GPIO_SDCC_FUNC_VAL, GPIO_PULL_UP, GPIO_10MA, 1);
	}
}

static void set_ramoops_buffer(void)
{
	uint64_t base, total_size, record_size;

	/*
	 * Hardcoded record and total sizes could be defined through Kconfig.
	 *
	 * The 'total_size' bytes of memory, aligned at 'record_size' boundary
	 * is found at the top of available memory as defined in the coreboot
	 * table and assigned to the ramoops cache.
	 *
	 * This is fairly brittle, as other parts of depthcharge or libpayload
	 * could be using this memory for something. But this is no wose than
	 * hardcoding this area to any particular address.
	 *
	 * A proper solution would be to have coreboot assign this memory and
	 * explixitly describe this in the coreboot memory table.
	 */
	record_size = 0x20000;
	total_size = 0x100000;
	base = 0;

	/* Let's allocate it as high as possible in the available memory */
	for (int i = 0; i < lib_sysinfo.n_memranges; i++) {
		uint64_t new_base, size;
		struct memrange *range = lib_sysinfo.memrange + i;

		size = range->size;
		if ((range->type != CB_MEM_RAM) ||
		    (size < (total_size + record_size)))
			continue;

		/* Record size aligned area is guaranteed to fit. */
		new_base = ALIGN_DOWN(range->base + size - total_size,
				      record_size);
		if (new_base > base)
			base = new_base;

	}
	if (base)
		ramoops_buffer(base, total_size, record_size);
}

static int board_setup(void)
{
	sysinfo_install_flags();

	fill_board_descriptor();

	fit_set_compat(bdescriptor.compat_string);

	install_phys_presence_flag();

	SpiController *spi = new_spi(0, 0);
	flash_set_ops(&new_spi_flash(&spi->ops)->ops);

	UsbHostController *usb_host1 = new_usb_hc(XHCI, 0x11000000);

	list_insert_after(&usb_host1->list_node, &usb_host_controllers);

	QcomMmcHost *mmc = new_qcom_mmc_host(1, MSM_SDC1_BASE, 8);
	if (!mmc)
		return -1;

	list_insert_after(&mmc->mmc.ctrlr.list_node,
			  &fixed_block_dev_controllers);

	Ipq806xI2c *i2c = new_ipq806x_i2c(GSBI_ID_1);
	tpm_set_ops(&new_slb9635_i2c(&i2c->ops, 0x20)->base.ops);

	flash_nvram_init();

	list_insert_after(&ipq_enet_fixup.list_node, &device_tree_fixups);

	set_ramoops_buffer();

	return 0;
}

int get_mach_id(void)
{
	int i;
	struct cb_mainboard *mainboard = lib_sysinfo.mainboard;
	const char *part_number = (const char *)mainboard->strings +
		mainboard->part_number_idx;

	struct PartDescriptor {
		const char *part_name;
		int mach_id;
	} parts[] = {
		{"Storm", 4936},
		{"AP148", CONFIG_MACHID},
	};

	for (i = 0; i < ARRAY_SIZE(parts); i++) {
		if (!strncmp(parts[i].part_name,
			     part_number, strlen(parts[i].part_name))) {
			return parts[i].mach_id;
		}
	}

	return -1;
}

INIT_FUNC(board_setup);
