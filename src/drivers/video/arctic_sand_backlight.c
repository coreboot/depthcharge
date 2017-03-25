/*
 * Copyright 2017 Google Inc.
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
 */
#include <libpayload.h>

#include "base/container_of.h"
#include "drivers/video/arctic_sand_backlight.h"

#define ARCXCNN_ILED_CONFIG	(0x05)  /* ILED Configuration */
#define ARCXCNN_LEDEN		(0x06)  /* LED Enable Register */
#define ARCXCNN_WLED_ISET_LSB	(0x07)  /* LED ISET LSB (in upper nibble) */
#define ARCXCNN_WLED_ISET_MSB	(0x08)  /* LED ISET MSB (8 bits) */

// Set backlight to 100% by default
#define DEFAULT_BL_BRIGHTNESS 0xFFF


static int arctic_sand_init(ArcticSandBacklight *me)
{
	return i2c_writeb(me->i2c_ops, me->chip_addr,
			  ARCXCNN_ILED_CONFIG, 0x57) ||
	       i2c_writeb(me->i2c_ops, me->chip_addr,
			  ARCXCNN_LEDEN, 0x3F);
}

static int arctic_sand_set_brightness(ArcticSandBacklight *me,
	uint16_t brightness)
{
	int ret;
	if (me->initialized == 0) {
		ret = arctic_sand_init(me);
		if (ret)
			return ret;
		me->initialized = 1;
	}
	return i2c_writeb(me->i2c_ops, me->chip_addr,
			  ARCXCNN_WLED_ISET_LSB, (brightness & 0xF) << 4) ||
	       i2c_writeb(me->i2c_ops, me->chip_addr,
			  ARCXCNN_WLED_ISET_MSB, brightness >> 4);
}

static int arctic_sand_switch_backlight(GpioOps *me, unsigned enable)
{
	ArcticSandBacklight *arctic_sand_bl =
		container_of(me, ArcticSandBacklight, gpio_ops);
	return arctic_sand_set_brightness(arctic_sand_bl,
					  enable ? DEFAULT_BL_BRIGHTNESS : 0);
}

GpioOps *new_arctic_sand_backlight(I2cOps *i2c_ops, uint8_t chip_addr)
{
	ArcticSandBacklight *arctic_sand_bl =
		xzalloc(sizeof(*arctic_sand_bl));
	arctic_sand_bl->i2c_ops = i2c_ops;
	arctic_sand_bl->gpio_ops.set = arctic_sand_switch_backlight;
	arctic_sand_bl->chip_addr = chip_addr;
	return &arctic_sand_bl->gpio_ops;
}
