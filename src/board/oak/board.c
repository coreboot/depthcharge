/*
 * Copyright 2013 Google Inc.
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

#include <assert.h>
#include <libpayload.h>

#include "base/init_funcs.h"
#include "boot/fit.h"
#include "boot/ramoops.h"
#include "config.h"
#include "drivers/bus/i2c/mtk_i2c.h"
#include "drivers/bus/spi/mt8173.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/ec/cros/spi.h"
#include "drivers/flash/spi.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/gpio/mtk_gpio.h"
#include "drivers/power/mt6397.h"
#include "drivers/sound/i2s.h"
#include "drivers/sound/max98090.h"
#include "drivers/tpm/slb9635_i2c.h"
#include "drivers/tpm/tpm.h"
#include "vboot/util/flag.h"

static int board_setup(void)
{
	sysinfo_install_flags(new_mtk_gpio_input);

	fit_set_compat("mediatek,mt8173-crosnb");

	MTKI2c *i2c2 = new_mtk_i2c(0x11009000, 0x11000200, 2, 0, 0x20,
				   ST_MODE, 100, 0);
	tpm_set_ops(&new_slb9635_i2c(&i2c2->ops, 0x20)->base.ops);

	MtkSpi *spibus = new_mtk_spi(0x1100A000);
	CrosEcSpiBus *cros_ec_spi_bus = new_cros_ec_spi_bus(&spibus->ops);
	cros_ec_set_bus(&cros_ec_spi_bus->ops);

	Mt6397Pmic *pmic = new_mt6397_power(0x1000D000, 0x10007000);
	power_set_ops(&pmic->ops);

	return 0;
}

INIT_FUNC(board_setup);
