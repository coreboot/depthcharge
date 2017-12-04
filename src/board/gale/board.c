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
#include "drivers/bus/spi/ipq40xx.h"
#include "drivers/bus/i2c/ipq40xx.h"
#include "drivers/bus/i2c/ipq40xx_blsp.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/gpio/gpio.h"
#include "drivers/gpio/ipq40xx.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/power.h"
#include "drivers/sound/route.h"
#include "drivers/storage/ipq40xx_mmc.h"
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

#define MSM_SDC1_BASE		0x7824000


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
	switch(lib_sysinfo.board_id) {
	case BOARD_ID_GALE_PROTO:
	case BOARD_ID_GALE_EVT:
	case BOARD_ID_GALE_EVT2_0:
	case BOARD_ID_GALE_EVT2_1:
		bdescriptor.compat_string = "google,gale";
		bdescriptor.calibration_needed = 1;
		break;
	case BOARD_ID_GALE_EVT3:
	default:
		bdescriptor.compat_string = "google,gale-v2";
		bdescriptor.calibration_needed = 1;
		break;
	}
}

static const DtPathMap mac_maps[] = {
	{ 0, "soc/edma@c080000/gmac0/local-mac-address" },
	{ 0, "soc/edma@c080000/gmac1/local-mac-address" },
	{}
};

static const DtPathMap calibration_maps[] = {
	{1, "soc/wifi@a000000/qcom,ath10k-pre-calibration-data",
	 "wifi_base64_calibration0"},
	{1, "soc/wifi@a800000/qcom,ath10k-pre-calibration-data",
	 "wifi_base64_calibration1"},
	{}
};

static const DtPathMap cc_maps[] = {
	{1, "soc/wifi@a000000", "qcom,ath10k-country-code"},
	{1, "soc/wifi@a800000", "qcom,ath10k-country-code"},
	{}
};

static int fix_device_tree(DeviceTreeFixup *fixup, DeviceTree *tree)
{
	int rv = 0;

	if ((lib_sysinfo.board_id >= BOARD_ID_GALE_EVT3) &&
		(lib_sysinfo.board_id != BOARD_ID_GALE_EVT2_1))
		rv = dt_set_mac_addresses(tree, mac_maps);

	if (bdescriptor.calibration_needed)
		rv |= dt_set_wifi_calibration(tree, calibration_maps);

	rv |= dt_set_wifi_country_code(tree, cc_maps);

	return rv;
}

static DeviceTreeFixup ipq_enet_fixup = {
	.fixup = fix_device_tree
};

gpio_func_data_t gale_mmc_gpio_conf[] = {
	{
		.gpio = 23,
		.func = 1,
		.pull = GPIO_PULL_UP,
		.drvstr = GPIO_10MA,
		.oe = GPIO_OE_DISABLE,
		.gpio_vm = GPIO_VM_ENABLE,
		.gpio_od_en = GPIO_OD_DISABLE,
		.gpio_pu_res = GPIO_PULL_RES2
	},
	{
		.gpio = 24,
		.func = 1,
		.pull = GPIO_PULL_UP,
		.drvstr = GPIO_10MA,
		.oe = GPIO_OE_DISABLE,
		.gpio_vm = GPIO_VM_ENABLE,
		.gpio_od_en = GPIO_OD_DISABLE,
		.gpio_pu_res = GPIO_PULL_RES2
	},
	{
		.gpio = 25,
		.func = 1,
		.pull = GPIO_PULL_UP,
		.drvstr = GPIO_10MA,
		.oe = GPIO_OE_DISABLE,
		.gpio_vm = GPIO_VM_ENABLE,
		.gpio_od_en = GPIO_OD_DISABLE,
		.gpio_pu_res = GPIO_PULL_RES2
	},
	{
		.gpio = 26,
		.func = 1,
		.pull = GPIO_PULL_UP,
		.drvstr = GPIO_10MA,
		.oe = GPIO_OE_DISABLE,
		.gpio_vm = GPIO_VM_ENABLE,
		.gpio_od_en = GPIO_OD_DISABLE,
		.gpio_pu_res = GPIO_PULL_RES2
	},
	{
		.gpio = 27,
		.func = 1,
		.pull = GPIO_PULL_UP,
		.drvstr = GPIO_4MA,
		.oe = GPIO_OE_DISABLE,
		.gpio_vm = GPIO_VM_ENABLE,
		.gpio_od_en = GPIO_OD_DISABLE,
		.gpio_pu_res = GPIO_PULL_RES2
	},
	{
		.gpio = 28,
		.func = 1,
		.pull = GPIO_PULL_UP,
		.drvstr = GPIO_10MA,
		.oe = GPIO_OE_DISABLE,
		.gpio_vm = GPIO_VM_ENABLE,
		.gpio_od_en = GPIO_OD_DISABLE,
		.gpio_pu_res = GPIO_PULL_RES2
	},
	{
		.gpio = 29,
		.func = 1,
		.pull = GPIO_PULL_UP,
		.drvstr = GPIO_10MA,
		.oe = GPIO_OE_DISABLE,
		.gpio_vm = GPIO_VM_ENABLE,
		.gpio_od_en = GPIO_OD_DISABLE,
		.gpio_pu_res = GPIO_PULL_RES2
	},
	{
		.gpio = 30,
		.func = 1,
		.pull = GPIO_PULL_UP,
		.drvstr = GPIO_10MA,
		.oe = GPIO_OE_DISABLE,
		.gpio_vm = GPIO_VM_ENABLE,
		.gpio_od_en = GPIO_OD_DISABLE,
		.gpio_pu_res = GPIO_PULL_RES2
	},
	{
		.gpio = 31,
		.func = 1,
		.pull = GPIO_PULL_UP,
		.drvstr = GPIO_10MA,
		.oe = GPIO_OE_DISABLE,
		.gpio_vm = GPIO_VM_ENABLE,
		.gpio_od_en = GPIO_OD_DISABLE,
		.gpio_pu_res = GPIO_PULL_RES2
	},
	{
		.gpio = 32,
		.func = 1,
		.pull = GPIO_PULL_UP,
		.drvstr = GPIO_10MA,
		.oe = GPIO_OE_DISABLE,
		.gpio_vm = GPIO_VM_ENABLE,
		.gpio_od_en = GPIO_OD_DISABLE,
		.gpio_pu_res = GPIO_PULL_RES2
	},
};

/* Ipq GPIO access wrapper. */
typedef struct
{
	GpioOps gpio_ops;	/* Depthcharge GPIO API wrapper. */
	gpio_t desc;		/* GPIO description. */
} IpqGpio;

static int get_gpio(struct GpioOps *me)
{
	IpqGpio *gpio = container_of(me, IpqGpio, gpio_ops);
	return gpio_get_in_value(gpio->desc);
}

static GpioOps *new_gpio_input_from_coreboot(uint32_t port)
{
	IpqGpio *gpio = xzalloc(sizeof(*gpio));
	gpio->gpio_ops.get = get_gpio;
	gpio->desc = (gpio_t)port;
	return &gpio->gpio_ops;
}

static void install_phys_presence_flag(void)
{
	GpioOps *phys_presence = sysinfo_lookup_gpio
		("developer", 1, new_gpio_input_from_coreboot);

	if (!phys_presence) {
		printf("%s failed retrieving phys presence GPIO\n", __func__);
		return;
	}
	flag_install(FLAG_PHYS_PRESENCE, phys_presence);
}

void ipq_configure_gpio(gpio_func_data_t *gpio, uint32_t count)
{
	int i;

	for (i = 0; i < count; i++) {
		gpio_tlmm_config(gpio->gpio, gpio->func, gpio->out,
				gpio->pull, gpio->drvstr, gpio->oe,
				gpio->gpio_vm, gpio->gpio_od_en,
				gpio->gpio_pu_res);
		gpio++;
	}
}

void board_mmc_gpio_config(void)
{
	ipq_configure_gpio(gale_mmc_gpio_conf, ARRAY_SIZE(gale_mmc_gpio_conf));
}

#if 0
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
#endif

static uint8_t kb_buffer[4];
static int kb_in, kb_out;

static int dakota_havekey(void)
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

static int dakota_getchar(void)
{
	int storm_char;

	while (!dakota_havekey())
		;

	storm_char = kb_buffer[kb_out++];

	kb_out %= sizeof(kb_buffer);

	return storm_char;
}

static struct console_input_driver dakota_input_driver =
{
	NULL,
	&dakota_havekey,
	&dakota_getchar
};

static void ipq_snoc_pnoc_init(void)
{
	/*
	 * IPQ40xx WiFi Global Config
	 * Bit 30:AXID_EN
	 * Enable AXI master bus Axid translating to confirm
	 * all txn submitted by order
	 * Bit 24: Use locally generated socslv_wxi_bvalid
	 * 1:  use locally generate socslv_wxi_bvalid for performance.
	 * 0:  use SNOC socslv_wxi_bvalid.	37
	 */
	write32(TCSR_WIFI0_GLB_CFG, 0x41000000);
	write32(TCSR_WIFI1_GLB_CFG, 0x41000000);

	/* IPQ40xx MEM_TYPE_SEL_M0_M2 Select Bit 26:24 - 2 NORMAL */
	write32(TCSR_PNOC_SNOC_MEMTYPE_M0_M2, 0x02222222);
}

static int board_setup(void)
{
	sysinfo_install_flags(NULL);

	fill_board_descriptor();

	fit_add_compat(bdescriptor.compat_string);

	install_phys_presence_flag();

	console_add_input_driver(&dakota_input_driver);

	power_set_ops(new_ipq40xx_power_ops());

	SpiController *spi = new_spi(0, 0);
	flash_set_ops(&new_spi_flash(&spi->ops)->ops);

	QcomMmcHost *mmc = new_qcom_mmc_host(1, MSM_SDC1_BASE, 8);

	if (!mmc)
		return -1;

	list_insert_after(&mmc->mmc.ctrlr.list_node,
				  &fixed_block_dev_controllers);

	UsbHostController *usb_host1 = new_usb_hc(XHCI, 0x8A00000);

	list_insert_after(&usb_host1->list_node, &usb_host_controllers);

#if (!CONFIG_MOCK_TPM)
	Ipq40xxI2c *i2c = new_ipq40xx_i2c(BLSP_QUP_ID_2);
	tpm_set_ops(&new_slb9635_i2c(&i2c->ops, 0x20)->base.ops);
#endif

	DisplayOps *ww_ring_ops = new_ww_ring_display
		(&new_ipq40xx_i2c (BLSP_QUP_ID_3)->ops, 0x32);

	display_set_ops(ww_ring_ops);

	display_init();

	write32(ADSS_AUDIO_TXB_CBCR_REG, 0); /* Disable ADSS clock branch */

	ipq_snoc_pnoc_init();

	list_insert_after(&ipq_enet_fixup.list_node, &device_tree_fixups);

	return 0;
}
INIT_FUNC(board_setup);
