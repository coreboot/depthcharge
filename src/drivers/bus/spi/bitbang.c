/*
 * Copyright 2018 Google LLC
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

#include <assert.h>
#include <libpayload.h>
#include "base/container_of.h"
#include "drivers/bus/spi/bitbang.h"

/* Set to 1 to dump all SPI transfers to the UART. */
#define TRACE		0
/*
 * In theory, this should work fine with 0 delay since the bus is fully clocked
 * by the master and the slave just needs to follow clock transitions whenever
 * they happen. We're not going to be "too fast" by bit-banging anyway. However,
 * if something doesn't work right, try increasing this value to slow it down.
 */
#define HALF_CLOCK_US	0

static int spi_transfer(SpiOps *me, void *in, const void *out, uint32_t size)
{
	BitbangSpi *bus = container_of(me, BitbangSpi, ops);

	if (TRACE) {
		if (in)
			printf("<");
		else
			printf(">");
	}

	while (size--) {
		int i;
		uint8_t in_byte = 0, out_byte = 0;
		if (out) {
			out_byte = *(const uint8_t *)out++;
			if (TRACE)
				printf("%02x", out_byte);
		}
		for (i = 7; i >= 0; i--) {
			gpio_set(bus->mosi, !!(out_byte & (1 << i)));
			if (HALF_CLOCK_US)
				udelay(HALF_CLOCK_US);
			gpio_set(bus->clk, 1);
			in_byte |= !!gpio_get(bus->miso) << i;
			if (HALF_CLOCK_US)
				udelay(HALF_CLOCK_US);
			gpio_set(bus->clk, 0);
		}
		if (in) {
			*(uint8_t *)in++ = in_byte;
			if (TRACE)
				printf("%02x", in_byte);
		}
	}

	if (TRACE)
		printf("\n");
	return 0;
}

static int spi_start(SpiOps *me)
{
	BitbangSpi *bus = container_of(me, BitbangSpi, ops);
	return gpio_set(bus->cs, 0);
}

static int spi_stop(SpiOps *me)
{
	BitbangSpi *bus = container_of(me, BitbangSpi, ops);
	return gpio_set(bus->cs, 1);
}

BitbangSpi *new_bitbang_spi(GpioOps *miso, GpioOps *mosi,
			    GpioOps *clk, GpioOps *cs)
{
	BitbangSpi *bus = xzalloc(sizeof(*bus));
	bus->miso = miso;
	bus->mosi = mosi;
	bus->clk = clk;
	bus->cs = cs;
	bus->ops.start = &spi_start;
	bus->ops.stop = &spi_stop;
	bus->ops.transfer = &spi_transfer;
	return bus;
}
