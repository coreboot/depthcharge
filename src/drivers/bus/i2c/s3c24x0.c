/*
 * Copyright 2013 Google Inc. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

#include <assert.h>
#include <libpayload.h>

#include "drivers/bus/i2c/i2c.h"
#include "drivers/gpio/gpio.h"
#include "drivers/gpio/s5p.h"

typedef struct __attribute__ ((packed)) I2cRegs
{
	uint8_t con;
	uint8_t _1[3];
	uint8_t stat;
	uint8_t _2[3];
	uint8_t add;
	uint8_t _3[3];
	uint8_t ds;
	uint8_t _4[3];
	uint8_t lc;
	uint8_t _5[3];
} I2cRegs;

enum {
	I2cConIntPending = 0x1 << 4,
	I2cConIntEn = 0x1 << 5,
	I2cConAckGen = 0x1 << 7
};

enum {
	I2cStatAck = 0x1 << 0,
	I2cStatAddrZero = 0x1 << 1,
	I2cStatAddrSlave = 0x1 << 2,
	I2cStatArb = 0x1 << 3,
	I2cStatEnable = 0x1 << 4,
	I2cStatStartStop = 0x1 << 5,
	I2cStatBusy = 0x1 << 5,

	I2cStatModeMask = 0x3 << 6,
	I2cStatSlaveRecv = 0x0 << 6,
	I2cStatSlaveXmit = 0x1 << 6,
	I2cStatMasterRecv = 0x2 << 6,
	I2cStatMasterXmit = 0x3 << 6
};

typedef struct I2cBus
{
	I2cRegs *regs;
	int ready;
} I2cBus;

static I2cBus i2c_busses[] = {
	{ (I2cRegs *)(uintptr_t)0x12c60000 },
	{ (I2cRegs *)(uintptr_t)0x12c70000 },
	{ (I2cRegs *)(uintptr_t)0x12c80000 },
	{ (I2cRegs *)(uintptr_t)0x12c90000 },
	{ (I2cRegs *)(uintptr_t)0x12ca0000 },
	{ (I2cRegs *)(uintptr_t)0x12cb0000 },
	{ (I2cRegs *)(uintptr_t)0x12cc0000 },
	{ (I2cRegs *)(uintptr_t)0x12cd0000 }
};

static const uint32_t *i2c_cfg = (uint32_t *)(uintptr_t)(0x10050000 + 0x234);

static int i2c_claim_bus(int bus)
{
	if (bus != 4)
		return 0;

	// Request the bus.
	if (gpio_set_value(s5p_gpio_index(GPIO_F, 0, 3), 0) < 0)
		return 1;

	// Wait for the EC to give it to us.
	int timeout = 2000 * 100; // 2s.
	while (timeout--) {
		int value = gpio_get_value(s5p_gpio_index(GPIO_E, 0, 4));
		if (value  < 0) {
			gpio_set_value(s5p_gpio_index(GPIO_F, 0, 3), 1);
			return 1;
		}
		if (value == 1)
			return 0;
		udelay(10);
	}

	// Time out, recind our request.
	gpio_set_value(s5p_gpio_index(GPIO_F, 0, 3), 1);

	return 1;
}

static void i2c_release_bus(int bus)
{
	if (bus != 4)
		return;

	gpio_set_value(s5p_gpio_index(GPIO_F, 0, 3), 1);

	return;
}

static int i2c_int_pending(I2cRegs *regs)
{
	return readb(&regs->con) & I2cConIntPending;
}

static void i2c_clear_int(I2cRegs *regs)
{
	writeb(readb(&regs->con) & ~I2cConIntPending, &regs->con);
}

static void i2c_ack_enable(I2cRegs *regs)
{
	writeb(readb(&regs->con) |= I2cConAckGen, &regs->con);
}

static void i2c_ack_disable(I2cRegs *regs)
{
	writeb(readb(&regs->con) &= ~I2cConAckGen, &regs->con);
}

static int i2c_got_ack(I2cRegs *regs)
{
	return !(readb(&regs->stat) & I2cStatAck);
}

static int i2c_init(I2cRegs *regs)
{

	writeb(I2cConIntEn | I2cConIntPending | 0x42, &regs->con);

	// Release the bus.
	if (gpio_set_value(s5p_gpio_index(GPIO_F, 0, 3), 1) < 0)
		return 1;

	// Switch from hi speed I2C to the normal one.
	writel(0x0, i2c_cfg);

	return 0;
}

static I2cRegs *i2c_get_regs(int bus_num)
{

	if (bus_num >= ARRAY_SIZE(i2c_busses)) {
		printf("Requested unknown I2C bus %d.\n", bus_num);
		return NULL;
	}

	I2cBus *bus = &i2c_busses[bus_num];
	if (!bus->ready) {
		if (i2c_init(bus->regs))
			return NULL;
	}
	bus->ready = 1;
	return bus->regs;
}

static int i2c_wait_for_idle(I2cRegs *regs)
{
	int timeout = 1000 * 100; // 1s.
	while (timeout--) {
		if (!(readb(&regs->stat) & I2cStatBusy))
			return 0;
		udelay(10);
	}
	printf("I2C timeout waiting for idle.\n");
	return 1;
}

static int i2c_wait_for_int(I2cRegs *regs)
{
	int timeout = 1000 * 100; // 1s.
	while (timeout--) {
		if (i2c_int_pending(regs))
			return 0;
		udelay(10);
	}
	printf("I2C timeout waiting for I2C interrupt.\n");
	return 1;
}




static int i2c_send_stop(I2cRegs *regs)
{
	uint8_t mode = readb(&regs->stat) & (I2cStatModeMask);
	writeb(mode | I2cStatEnable, &regs->stat);
	i2c_clear_int(regs);
	return i2c_wait_for_idle(regs);
}

static int i2c_send_start(I2cRegs *regs, int read, int chip)
{
	writeb(chip << 1, &regs->ds);
	uint8_t mode = read ? I2cStatMasterRecv : I2cStatMasterXmit;
	writeb(mode | I2cStatStartStop | I2cStatEnable, &regs->stat);
	i2c_clear_int(regs);

	if (i2c_wait_for_int(regs))
		return 1;

	if (!i2c_got_ack(regs)) {
		// Nobody home, but they may just be asleep.
		return 1;
	}

	return 0;
}

static int i2c_xmit_buf(I2cRegs *regs, uint8_t *data, int len)
{
	assert(len);

	i2c_ack_enable(regs);

	for (int i = 0; i < len; i++) {
		writeb(data[i], &regs->ds);

		i2c_clear_int(regs);
		if (i2c_wait_for_int(regs))
			return 1;

		if (!i2c_got_ack(regs)) {
			printf("I2c nacked.\n");
			return 1;
		}
	}

	return 0;
}

static int i2c_recv_buf(I2cRegs *regs, uint8_t *data, int len)
{
	assert(len);

	i2c_ack_enable(regs);

	for (int i = 0; i < len; i++) {
		if (i == len - 1)
			i2c_ack_disable(regs);

		i2c_clear_int(regs);
		if (i2c_wait_for_int(regs))
			return 1;

		data[i] = readb(&regs->ds);
	}

	return 0;
}

static int i2c_readwrite(I2cRegs *regs, int read, uint8_t chip, uint32_t addr,
			 int addr_len, uint8_t *data, int data_len)
{
	if (!regs || i2c_wait_for_idle(regs))
		return 1;

	if (data_len <= 0 || addr_len < 0 || addr_len > sizeof(addr)) {
		printf("%s: Bad parameters.\n", __func__);
		return 1;
	}

	writeb(I2cStatMasterXmit | I2cStatEnable, &regs->stat);

	if (addr_len) {
		uint8_t addr_buf[sizeof(addr)];
		for (int i = 0; i < addr_len; i++) {
			int byte = addr_len - 1 - i;
			addr_buf[i] = addr >> (byte * 8);
		}

		if (i2c_send_start(regs, 0, chip) ||
			i2c_xmit_buf(regs, addr_buf, addr_len))
			return 1;
	}

	// Send a start if we haven't yet, or we need to go from write to read.
	if (!addr_len || read)
		if (i2c_send_start(regs, read, chip))
			return 1;

	if (read) {
		if (i2c_recv_buf(regs, data, data_len))
			return 1;
	} else {
		if (i2c_xmit_buf(regs, data, data_len))
			return 1;
	}

	return 0;
}

int i2c_read(uint8_t bus, uint8_t chip, uint32_t addr, int addr_len,
	     uint8_t *data, int data_len)
{
	if (i2c_claim_bus(bus))
		return 1;

	I2cRegs *regs = i2c_get_regs(bus);
	int res = i2c_readwrite(regs, 1, chip, addr, addr_len, data, data_len);
	res = i2c_send_stop(regs) || res;
	i2c_release_bus(bus);
	return res;
}

int i2c_write(uint8_t bus, uint8_t chip, uint32_t addr, int addr_len,
	      uint8_t *data, int data_len)
{
	if (i2c_claim_bus(bus))
		return 1;

	I2cRegs *regs = i2c_get_regs(bus);
	int res = i2c_readwrite(regs, 0, chip, addr, addr_len, data, data_len);
	res = i2c_send_stop(regs) || res;
	i2c_release_bus(bus);
	return res;
}
