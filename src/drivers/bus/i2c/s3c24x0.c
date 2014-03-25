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

#include "base/container_of.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/bus/i2c/s3c24x0.h"

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
	writeb(readb(&regs->con) | I2cConAckGen, &regs->con);
}

static void i2c_ack_disable(I2cRegs *regs)
{
	writeb(readb(&regs->con) & ~I2cConAckGen, &regs->con);
}

static int i2c_got_ack(I2cRegs *regs)
{
	return !(readb(&regs->stat) & I2cStatAck);
}

static int i2c_init(I2cRegs *regs)
{
	writeb(I2cConIntEn | I2cConIntPending | 0x42, &regs->con);
	return 0;
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

static int i2c_transfer(I2cOps *me, I2cSeg *segments, int seg_count)
{
	S3c24x0I2c *bus = container_of(me, S3c24x0I2c, ops);
	I2cRegs *regs = bus->reg_addr;
	int res = 0;

	if (!bus->ready) {
		if (i2c_init(regs))
			return 1;
		bus->ready = 1;
	}

	if (!regs || i2c_wait_for_idle(regs))
		return 1;

	writeb(I2cStatMasterXmit | I2cStatEnable, &regs->stat);

	for (int i = 0; i < seg_count; i++) {
		I2cSeg *seg = &segments[i];

		res = i2c_send_start(regs, seg->read, seg->chip);
		if (res)
			break;
		if (seg->read)
			res = i2c_recv_buf(regs, seg->buf, seg->len);
		else
			res = i2c_xmit_buf(regs, seg->buf, seg->len);
		if (res)
			break;
	}

	return i2c_send_stop(regs) || res;
}

S3c24x0I2c *new_s3c24x0_i2c(uintptr_t reg_addr)
{
	S3c24x0I2c *bus = xzalloc(sizeof(*bus));
	bus->ops.transfer = &i2c_transfer;
	bus->reg_addr = (void *)reg_addr;
	return bus;
}
