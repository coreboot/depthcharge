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

static const uint32_t *gpa2con = (uint32_t *)(uintptr_t)(0x11400000 + 0x40);
static const uint32_t *gpa2pud = (uint32_t *)(uintptr_t)(0x11400000 + 0x48);
static const uint32_t *gpe0con = (uint32_t *)(uintptr_t)(0x13400000 + 0x00);
static const uint32_t *gpe0dat = (uint32_t *)(uintptr_t)(0x13400000 + 0x04);
static const uint32_t *gpe0pud = (uint32_t *)(uintptr_t)(0x13400000 + 0x08);
static const uint32_t *gpf0con = (uint32_t *)(uintptr_t)(0x13400000 + 0x40);
static const uint32_t *gpf0dat = (uint32_t *)(uintptr_t)(0x13400000 + 0x48);
static const uint32_t *gpf0pud = (uint32_t *)(uintptr_t)(0x13400000 + 0x48);
static const uint32_t *i2c_cfg = (uint32_t *)(uintptr_t)(0x10050000 + 0x234);

static int i2c_claim_bus(int bus)
{
	if (bus != 4)
		return 0;

	// Request the bus.
	writel(readl(gpf0dat) & ~0x8, gpf0dat);

	// Wait for the EC to give it to us.
	int timeout = 200 * 1000; // 200ms.
	while (timeout--) {
		if (readl(gpe0dat) & 0x10)
			return 0;
		udelay(10);
	}

	// Time out, recind our request.
	writel(readl(gpf0dat) | 0x8, gpf0dat);

	return 1;
}

static void i2c_release_bus(int bus)
{
	if (bus != 4)
		return;

	writel(readl(gpf0dat) | 0x8, gpf0dat);

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

	writeb(I2cConIntEn | I2cConIntPending | 0x46, &regs->con);

	// Set gpf0dat[3] to 1 to release the bus.
	writel(readl(gpf0dat) | 0x8, gpf0dat);

	// Set gpa2con[0] and [1] set to 3 for I2C.
	writel((readl(gpa2con) & ~0xff) | 0x33, gpa2con);
	// Set gpf0con[3] to output.
	writel((readl(gpf0con) & ~0xf000) | 0x1000, gpf0con);
	// Set gpe0con[4] to input.
	writel(readl(gpe0con) & ~0xf0000, gpe0con);

	// Turn off pull-ups/downs.
	writel(readl(gpa2pud) & ~0xf, gpa2pud);
	writel(readl(gpe0pud) & ~(0x3 << (2 * 4)), gpe0pud);
	writel(readl(gpf0pud) & ~(0x3 << (2 * 3)), gpf0pud);

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
	int timeout = 100 * 1000; // 100ms.
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
	int timeout = 100 * 1000; // 100ms.
	while (timeout && !i2c_int_pending(regs))
		timeout--;
	if (!timeout)
		printf("I2C timeout waiting for I2C interrupt.\n");
	return !timeout;
}

static int i2c_send_stop(I2cRegs *regs)
{
	uint8_t mode = readb(&regs->stat) & (I2cStatModeMask);
	writeb(mode | I2cStatEnable, &regs->stat);
	i2c_clear_int(regs);
	return i2c_wait_for_idle(regs);
}



static int i2c_xmit_buf(I2cRegs *regs, int read, int chip, uint8_t *data,
			int len)
{
	assert(len);

	writeb(chip << 1, &regs->ds);
	uint8_t mode = read ? I2cStatMasterRecv : I2cStatMasterXmit;
	writeb(mode | I2cStatStartStop | I2cStatEnable, &regs->stat);

	if (i2c_wait_for_int(regs))
		return 1;

	if (!i2c_got_ack(regs)) {
		printf("I2c nacked.\n");
		return 1;
	}

	i2c_ack_enable(regs);

	for (int i = 0; i < len; i++) {
		if (read) {
			if (i == len - 1)
				i2c_ack_disable(regs);
		} else {
			writeb(data[i], &regs->ds);
		}

		i2c_clear_int(regs);
		if (i2c_wait_for_int(regs))
			return 1;

		if (read) {
			data[i] = readb(&regs->ds);
		} else {
			if (!i2c_got_ack(regs)) {
				printf("I2c nacked.\n");
				return 1;
			}
		}
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

		if (i2c_xmit_buf(regs, 0, chip, addr_buf, addr_len))
			return 1;
	}

	if (i2c_xmit_buf(regs, read, chip, data, data_len))
		return 1;

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
