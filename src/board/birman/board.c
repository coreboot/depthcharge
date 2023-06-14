// SPDX-License-Identifier: GPL-2.0
/* Copyright 2023 Google LLC.  */

#include <libpayload.h>

#include "base/init_funcs.h"
#include "boot/commandline.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/fch.h"
#include "drivers/soc/phoenix.h"
#include "drivers/storage/nvme.h"
#include "drivers/storage/storage_common.h"
#include "pci.h"
#include "vboot/util/flag.h"

__weak const struct storage_config *variant_get_storage_configs(size_t *count)
{
	static const struct storage_config storage_configs[] = {
		/* Coreboot devicetree.cb: gpp_bridge_1_2 = 01.2 */
		{ .media = STORAGE_NVME, .pci_dev = PCI_DEV(0, 0x01, 0x2)},
		/* Coreboot devicetree.cb: gpp_bridge_2_4 = 02.4 */
		{ .media = STORAGE_NVME, .pci_dev = PCI_DEV(0, 0x02, 0x4)},
	};

	*count = ARRAY_SIZE(storage_configs);
	return storage_configs;
}

static int get_gpio_0(struct GpioOps *me) {
	return 0;
}

static struct GpioOps fake_gpio_0 = {
	.get = get_gpio_0,
};


static int get_gpio_1(struct GpioOps *me) {
	return 1;
}

static struct GpioOps fake_gpio_1 = {
	.get = get_gpio_1,
};

static int board_setup(void)
{
	sysinfo_install_flags(NULL);
	flag_replace(FLAG_LIDSW, &fake_gpio_1);
	flag_replace(FLAG_PWRSW, &fake_gpio_0);

	power_set_ops(&kern_power_ops);

	return 0;
}

INIT_FUNC(board_setup);
