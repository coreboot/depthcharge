/*
 * Copyright 2013 Google LLC
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
 */

#include <libpayload.h>

#include "drivers/gpio/gpio.h"

typedef struct {
	GpioOps ops;
	GpioOps *a;
} GpioUnaryOp;

typedef struct {
	GpioOps ops;
	GpioOps *a;
	GpioOps *b;
} GpioBinaryOp;

static int gpio_not_get(GpioOps *me)
{
	GpioUnaryOp *op = container_of(me, GpioUnaryOp, ops);
	return !gpio_get(op->a);
}

static int gpio_not_set(GpioOps *me, unsigned value)
{
	GpioUnaryOp *op = container_of(me, GpioUnaryOp, ops);
	return gpio_set(op->a, !value);
}

static int gpio_and_get(GpioOps *me)
{
	GpioBinaryOp *op = container_of(me, GpioBinaryOp, ops);
	return gpio_get(op->a) && gpio_get(op->b);
}

static int gpio_or_get(GpioOps *me)
{
	GpioBinaryOp *op = container_of(me, GpioBinaryOp, ops);
	return gpio_get(op->a) || gpio_get(op->b);
}

GpioOps *new_gpio_not(GpioOps *a)
{
	GpioUnaryOp *op = xzalloc(sizeof(*op));
	op->ops.get = &gpio_not_get;
	op->ops.set = &gpio_not_set;
	op->a = a;
	return &op->ops;
}

static int gpio_passthrough_get(GpioOps *me)
{
	GpioUnaryOp *op = container_of(me, GpioUnaryOp, ops);
	return gpio_get(op->a);
}

static int gpio_passthrough_set(GpioOps *me, unsigned value)
{
	GpioUnaryOp *op = container_of(me, GpioUnaryOp, ops);
	return gpio_set(op->a, value);
}

GpioOps *new_gpio_passthrough(GpioOps *a)
{
	GpioUnaryOp *op = xzalloc(sizeof(*op));
	op->ops.get = &gpio_passthrough_get;
	op->ops.set = &gpio_passthrough_set;
	op->a = a;
	return &op->ops;
}

static GpioOps *new_gpio_binary_op(GpioOps *a, GpioOps *b,
				   int (*get)(GpioOps *me))
{
	GpioBinaryOp *op = xzalloc(sizeof(*op));
	op->ops.get = get;
	op->a = a;
	op->b = b;
	return &op->ops;
}

static int return_high(GpioOps *me)
{
	return 1;
}

GpioOps *new_gpio_high(void)
{
	GpioOps *ops = xzalloc(sizeof(*ops));
	ops->get = return_high;
	return ops;
}

GpioOps *new_gpio_low(void)
{
	return new_gpio_not(new_gpio_high());
}

GpioOps *new_gpio_and(GpioOps *a, GpioOps *b)
{
	return new_gpio_binary_op(a, b, &gpio_and_get);
}

GpioOps *new_gpio_or(GpioOps *a, GpioOps *b)
{
	return new_gpio_binary_op(a, b, &gpio_or_get);
}

typedef struct {
	GpioBinaryOp binary;
	unsigned int rising_delay_us;
	unsigned int falling_delay_us;
} GpioSplitterOp;

static int gpio_splitter_set(struct GpioOps *me, unsigned int value)
{
	GpioSplitterOp *splitter = container_of(me, GpioSplitterOp, binary.ops);
	int ret = 0;

	if (value) {
		ret |= gpio_set(splitter->binary.a, value);
		udelay(splitter->rising_delay_us);
		ret |= gpio_set(splitter->binary.b, value);
	} else {
		ret |= gpio_set(splitter->binary.b, value);
		udelay(splitter->falling_delay_us);
		ret |= gpio_set(splitter->binary.a, value);
	}

	return ret;
}

GpioOps *new_gpio_delayed_splitter(GpioOps *a, GpioOps *b,
		unsigned int rising_delay_us, unsigned int falling_delay_us)
{
	GpioSplitterOp *splitter = xzalloc(sizeof(*splitter));
	splitter->binary.ops.set = &gpio_splitter_set;
	splitter->binary.a = a;
	splitter->binary.b = b;
	splitter->rising_delay_us = rising_delay_us;
	splitter->falling_delay_us = falling_delay_us;
	return &splitter->binary.ops;

}
