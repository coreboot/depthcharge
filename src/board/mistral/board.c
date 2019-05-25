/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2018, The Linux Foundation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <assert.h>
#include <libpayload.h>
#include <stdio.h>
#include <stdint.h>

#include "base/init_funcs.h"
#include "drivers/gpio/gpio.h"
#include "vboot/util/flag.h"
#include "boot/fit.h"
#include "drivers/bus/usb/usb.h"

static const VpdDeviceTreeMap vpd_dt_map[] = {
	{ "cherokee_mac", "/soc@0/wifi@a000000/local-mac-address" },
	{ "eth_wan_mac", "/soc@0/ethernet@7a80000/mdio/switch0@16/ports/port@2/local-mac-address" },
	{ "eth_lan_mac", "/soc@0/ethernet@7a80000/mdio/switch0@16/ports/port@3/local-mac-address" },
	{ "bluetooth_mac", "/soc@0/serial@78b2000/wcn3990-bt/local-bd-address" },
	{ "cascade_mac", "/soc@0/pci@10000000/pcie@0/wifi@0,0/local-mac-address" },

	{}
};

static const DtPathMap calibration_maps[] = {
	{1, "/soc@0/pci@10000000/pcie@0/wifi@0,0/qcom,ath10k-pre-calibration-data",
	"wifi_base64_calibration0"},
	{}
};

static const DtPathMap cc_maps[] = {
	{1, "/soc@0/pci@10000000/pcie@0/wifi@0,0", "qcom,ath10k-country-code"},
	{1, "/soc@0/wifi@a000000", "qcom,ath10k-country-code"},
	{}
};

static const DtPathMap xo_cal_map[] = {
	{1, "/soc@0/wifi@a000000", "xo-cal-data" },
	{}
};

static int fix_device_tree(DeviceTreeFixup *fixup, DeviceTree *tree)
{
	int rv = 0;

	rv = dt_set_wifi_calibration(tree, calibration_maps);
	rv |= dt_set_wifi_country_code(tree, cc_maps);
	rv |= dt_set_xo_cal_data(tree, xo_cal_map);

	return rv;
}

static DeviceTreeFixup ipq_enet_fixup = {
	.fixup = fix_device_tree
};

static int board_setup(void)
{
	/* stub out required GPIOs for vboot */
	flag_replace(FLAG_LIDSW, new_gpio_high());
	flag_replace(FLAG_PWRSW, new_gpio_low());

	UsbHostController *usb_host2 = new_usb_hc(XHCI, 0x78C0000);

	list_insert_after(&usb_host2->list_node, &usb_host_controllers);
	fit_add_compat("qcom,qcs404-evb");

	dt_register_vpd_mac_fixup(vpd_dt_map);

	list_insert_after(&ipq_enet_fixup.list_node, &device_tree_fixups);

	return 0;
}

INIT_FUNC(board_setup);
