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

#include <arch/io.h>

#include "base/init_funcs.h"
#include "board/daisy/i2c_arb.h"
#include "drivers/bus/i2c/s3c24x0.h"
#include "drivers/ec/chromeos/mkbp.h"
#include "drivers/gpio/s5p.h"
#include "drivers/sound/max98095.h"
#include "drivers/tpm/tpm.h"
#include "vboot/util/flag.h"

static uint32_t *i2c_cfg = (uint32_t *)(uintptr_t)(0x10050000 + 0x234);

static int board_setup(void)
{
	S5pGpio *ec_in_rw = new_s5p_gpio_input(GPIO_D, 1, 7);
	if (!ec_in_rw)
		return 1;
	if (flag_install(FLAG_ECINRW, &ec_in_rw->ops))
		return 1;

	// Switch from hi speed I2C to the normal one.
	writel(0x0, i2c_cfg);

	S3c24x0I2c *i2c3 = new_s3c24x0_i2c((void *)(uintptr_t)0x12c90000);
	S3c24x0I2c *i2c4 = new_s3c24x0_i2c((void *)(uintptr_t)0x12ca0000);
	S3c24x0I2c *i2c7 = new_s3c24x0_i2c((void *)(uintptr_t)0x12cd0000);
	if (!i2c3 || !i2c4 || !i2c7)
		return 1;

	S5pGpio *request_gpio = new_s5p_gpio_output(GPIO_F, 0, 3);
	S5pGpio *grant_gpio = new_s5p_gpio_input(GPIO_E, 0, 4);
	if (!request_gpio || !grant_gpio)
		return 1;
	SnowI2cArb *arb4 = new_snow_i2c_arb(&i2c4->ops, &request_gpio->ops,
					    &grant_gpio->ops);
	if (!arb4)
		return 1;

	tis_set_i2c_bus(&i2c3->ops);
	mkbp_set_i2c_bus(&arb4->ops);
	max98095_set_i2c_bus(&i2c7->ops);

	return 0;
}

INIT_FUNC(board_setup);
