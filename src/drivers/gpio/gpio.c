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
 */

#include <libpayload.h>

#include "base/container_of.h"
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
