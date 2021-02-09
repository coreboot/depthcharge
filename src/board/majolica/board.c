// SPDX-License-Identifier: GPL-2.0
/* Copyright 2021 Google LLC.  */

#include <libpayload.h>

#include "base/init_funcs.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/fch.h"
#include "drivers/soc/cezanne.h"
#include "vboot/util/flag.h"


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
