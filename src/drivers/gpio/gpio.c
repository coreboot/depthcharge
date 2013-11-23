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
	return !op->a->get(op->a);
}

static int gpio_and_get(GpioOps *me)
{
	GpioBinaryOp *op = container_of(me, GpioBinaryOp, ops);
	return op->a->get(op->a) && op->b->get(op->b);
}

static int gpio_or_get(GpioOps *me)
{
	GpioBinaryOp *op = container_of(me, GpioBinaryOp, ops);
	return op->a->get(op->a) || op->b->get(op->b);
}

GpioOps *new_gpio_not(GpioOps *a)
{
	GpioUnaryOp *op = xzalloc(sizeof(*op));
	op->ops.get = &gpio_not_get;
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

GpioOps *new_gpio_and(GpioOps *a, GpioOps *b)
{
	return new_gpio_binary_op(a, b, &gpio_and_get);
}

GpioOps *new_gpio_or(GpioOps *a, GpioOps *b)
{
	return new_gpio_binary_op(a, b, &gpio_or_get);
}
