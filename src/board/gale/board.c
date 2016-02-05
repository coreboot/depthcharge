/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
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
#include <stdint.h>

#include "base/init_funcs.h"
#include "boot/fit.h"
#include "boot/ramoops.h"
#include "drivers/bus/i2c/ipq806x.h"
#include "drivers/bus/i2c/ipq806x_gsbi.h"
#include "drivers/bus/spi/ipq806x.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/gpio/gpio.h"
#include "drivers/gpio/ipq806x.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/ipq806x.h"
#include "drivers/sound/ipq806x.h"
#include "drivers/sound/route.h"
#include "drivers/storage/ipq806x_mmc.h"
#include "drivers/storage/mtd/mtd.h"
#include "drivers/storage/mtd/nand/ipq_nand.h"
#include "drivers/storage/mtd/stream.h"
#include "drivers/storage/spi_gpt.h"
#include "drivers/tpm/slb9635_i2c.h"
#include "drivers/tpm/tpm.h"
#include "drivers/video/ww_ring.h"
#include "vboot/callbacks/nvstorage_flash.h"
#include "vboot/stages.h"
#include "vboot/util/flag.h"

#include "board.h"

#define GPIO_SDCC_FUNC_VAL      2
#define GPIO_I2S_FUNC_VAL       1

#define MSM_SDC1_BASE		0x12400000

#define EBI2ND_BASE		0x1ac00000

/* Structure describing properties of various Storm based boards. */
struct board_descriptor {
	const char *compat_string; // Match the device tree in FIT image.
	int calibration_needed;	   // Some boards need to populate WiFi
				   // calibration data.
	int use_nand;		   // true if NAND, false if eMMC
};

static struct board_descriptor bdescriptor;

static void fill_board_descriptor(void)
{
	int storm_board = 0;
	bdescriptor.use_nand = 0;

	switch(lib_sysinfo.board_id) {
	case BOARD_ID_WHIRLWIND_SP3:
		bdescriptor.compat_string = "google,whirlwind-sp3";
		bdescriptor.calibration_needed = 1;
		break;

	case BOARD_ID_WHIRLWIND_SP5:
		bdescriptor.compat_string = "google,whirlwind-sp5";
		bdescriptor.calibration_needed = 1;
		break;

	case BOARD_ID_PROTO_0_2_NAND:
		bdescriptor.use_nand = 1;
		storm_board = 1;
		break;

	case BOARD_ID_PROTO_0:
	case BOARD_ID_PROTO_0_2:
		storm_board = 1;
		break;

	default:
		printf("Unknown board id %d; assuming like Storm Proto 0.2\n",
		       lib_sysinfo.board_id);
		storm_board = 1;
		break;
	}

	if (storm_board) {
		bdescriptor.compat_string = "google,storm-proto0";
		bdescriptor.calibration_needed = 0;
	}
}

static const DtPathMap mac_maps[] = {
	{ 0, "soc/ethernet@37000000/local-mac-address" },
	{ 0, "soc/ethernet@37400000/local-mac-address" },
	{ 1, "chosen/bluetooth/local-mac-address" },
	{}
};

static const DtPathMap calibration_maps[] = {
	{1, "soc/pci@1b500000/pcie@0/ath10k@0,0/qcom,ath10k-calibration-data",
	 "wifi_calibration0"},
	{1, "soc/pci@1b700000/pcie@0/ath10k@0,0/qcom,ath10k-calibration-data",
	 "wifi_calibration1"},
	{1, "soc/pci@1b900000/pcie@0/ath10k@0,0/qcom,ath10k-calibration-data",
	 "wifi_calibration2"},
	{}
};

static int fix_device_tree(DeviceTreeFixup *fixup, DeviceTree *tree)
{
	int rv;

	rv = dt_set_mac_addresses(tree, mac_maps);

	if (bdescriptor.calibration_needed)
		rv |= dt_set_wifi_calibration(tree, calibration_maps);

	return rv;
}

static DeviceTreeFixup ipq_enet_fixup = {
	.fixup = fix_device_tree
};

/* DAC GPIO assignment. */
enum storm_dac_gpio {
	DAC_SDMODE = 25,
};

/* I2S bus GPIO assignments. */
enum storm_i2s_gpio {
	I2S_SYNC = 27,
	I2S_CLK = 28,
	I2S_DOUT = 32,
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
	gpio_t desc;		/* GPIO description. */
} StormGpio;

static int get_gpio(struct GpioOps *me)
{
	StormGpio *gpio = container_of(me, StormGpio, gpio_ops);
	return gpio_get_in_value(gpio->desc);
}

static int set_gpio(struct GpioOps *me, unsigned value)
{
	StormGpio *gpio = container_of(me, StormGpio, gpio_ops);
	gpio_set_out_value(gpio->desc, value);
	return 0;
}

static GpioOps *new_storm_dac_gpio_output()
{
	StormGpio *gpio = xzalloc(sizeof(*gpio));
	gpio->gpio_ops.set = set_gpio;
	gpio->desc = (gpio_t)DAC_SDMODE;
	return &gpio->gpio_ops;
}

static GpioOps *new_storm_gpio_input_from_coreboot(uint32_t port)
{
	StormGpio *gpio = xzalloc(sizeof(*gpio));
	gpio->gpio_ops.get = get_gpio;
	gpio->desc = (gpio_t)port;
	return &gpio->gpio_ops;
}

static void install_phys_presence_flag(void)
{
	GpioOps *phys_presence = sysinfo_lookup_gpio
		("developer", 1, new_storm_gpio_input_from_coreboot);

	if (!phys_presence) {
		printf("%s failed retrieving phys presence GPIO\n", __func__);
		return;
	}

	flag_install(FLAG_PHYS_PRESENCE, phys_presence);
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

void board_i2s_gpio_config(void)
{
	unsigned i;
	unsigned char gpio_config_arr[] = {I2S_SYNC, I2S_CLK, I2S_DOUT};

	for (i = 0; i < ARRAY_SIZE(gpio_config_arr); i++) {
		gpio_tlmm_config_set(gpio_config_arr[i], GPIO_I2S_FUNC_VAL,
				GPIO_NO_PULL, GPIO_16MA, 1);
	}
}

void board_dac_gpio_config(void)
{
	gpio_tlmm_config_set(DAC_SDMODE, FUNC_SEL_GPIO, GPIO_NO_PULL,
			GPIO_16MA, 1);
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
	 * could be using this memory for something. But this is no worse than
	 * hardcoding this area to any particular address.
	 *
	 * A proper solution would be to have coreboot assign this memory and
	 * explicitly describe this in the coreboot memory table.
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

static uint8_t kb_buffer[4];
static int kb_in, kb_out;

static int storm_havekey(void)
{
	/*
	 * We want to react to the button press only, i.e. we need to
	 * catch the "unpressed -> pressed" transition.
	 */
	static uint32_t prev = 1;
	uint32_t rv = flag_fetch(FLAG_PHYS_PRESENCE);

	if (prev == rv)
		return kb_in != kb_out;

	prev = rv;
	if (!rv)
		return kb_in != kb_out;

	if (((kb_in + 1) % sizeof(kb_buffer)) == kb_out) {
		printf("%s: keyboard buffer overflow!\n", __func__);
		return 0;
	}

	/* Dev switch was pressed, what's the meaning of it? */
	if (vboot_in_recovery()) {
		/* This must mean ^D, the user wants to switch to dev mode. */
		kb_buffer[kb_in++] = 0x4;
		kb_in %= sizeof(kb_buffer);

		if (((kb_in + 1) % sizeof(kb_buffer)) != kb_out)
			kb_buffer[kb_in++] = 0xd;
		else
			/*
			 * Should never happen, but worse come to worse the
			 * user will lose the CR and will have to reboot in
			 * recovery mode again to enter dev mode.
			 */
			printf("%s: keyboard buffer overflow!\n", __func__);
	} else {
		/* This must mean ^U, the user wants to boot from USB. */
		kb_buffer[kb_in++] = 0x15;
	}

	kb_in %= sizeof(kb_buffer);

	return 1;
}

static int storm_getchar(void)
{
	int storm_char;

	while (!storm_havekey())
		;

	storm_char = kb_buffer[kb_out++];

	kb_out %= sizeof(kb_buffer);

	return storm_char;
}

static struct console_input_driver storm_input_driver =
{
	NULL,
	&storm_havekey,
	&storm_getchar
};

static int board_setup(void)
{
	sysinfo_install_flags(NULL);

	fill_board_descriptor();

	fit_set_compat(bdescriptor.compat_string);

	install_phys_presence_flag();

	console_add_input_driver(&storm_input_driver);

	power_set_ops(new_ipq806x_power_ops());

	SpiController *spi = new_spi(0, 0);
	flash_set_ops(&new_spi_flash(&spi->ops)->ops);

	UsbHostController *usb_host1 = new_usb_hc(XHCI, 0x11000000);

	list_insert_after(&usb_host1->list_node, &usb_host_controllers);

	if (bdescriptor.use_nand) {
		MtdDevCtrlr *mtd = new_ipq_nand((void *)EBI2ND_BASE);
		SpiGptCtrlr *virtual_dev = new_spi_gpt("RW_GPT",
						       new_mtd_stream(mtd),
						       "soc/nand@0x1ac00000");
		list_insert_after(&virtual_dev->block_ctrlr.list_node,
				  &fixed_block_dev_controllers);
	} else {
		QcomMmcHost *mmc = new_qcom_mmc_host(1, MSM_SDC1_BASE, 8);
		if (!mmc)
			return -1;

		list_insert_after(&mmc->mmc.ctrlr.list_node,
				  &fixed_block_dev_controllers);
	}

	Ipq806xI2c *i2c = new_ipq806x_i2c(GSBI_ID_1);
	tpm_set_ops(&new_slb9635_i2c(&i2c->ops, 0x20)->base.ops);

	if (lib_sysinfo.board_id >= BOARD_ID_WHIRLWIND_SP5) {

		DisplayOps *ww_ring_ops = new_ww_ring_display
			(&new_ipq806x_i2c (GSBI_ID_7)->ops, 0x32);

		display_set_ops(ww_ring_ops);

		/*
		 * Explicit initialization is required because the ring could
		 * be in an arbitrary state before the system is restarted,
		 * and board reset would not affect the state of the ring
		 * attached over i2c.
		 */
		display_init();
	}

	Ipq806xSound *sound = new_ipq806x_sound(new_storm_dac_gpio_output(),
			48000, 2, 16, 1000);
	SoundRoute *sound_route = new_sound_route(&sound->ops);
	sound_set_ops(&sound_route->ops);

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

int board_wan_port_number(void)
{
	if ((lib_sysinfo.board_id == BOARD_ID_PROTO_0) ||
	    (lib_sysinfo.board_id == BOARD_ID_PROTO_0_2))
		return 4; /* Storm variants */

	return 1; /* Whirlwind variants, let it be the default. */
}

INIT_FUNC(board_setup);
