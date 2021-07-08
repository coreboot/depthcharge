/*
 * This file is part of the depthcharge project.
 *
 * Copyright (C) 2018-2019, The Linux Foundation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "drivers/bus/i2c/qcom_qupv3_i2c.h"

static int i2c_do_xfer(QupRegs *reg_addr, I2cSeg segment, unsigned int prams)
{
	unsigned int cmd = segment.read ? 2 : 1;
	unsigned int master_cmd_reg_val = (cmd << M_OPCODE_SHFT);
	void *dout = NULL, *din = NULL;

	if (!segment.read) {
		write32(&reg_addr->tx_trans_len.i2c_tx_trans_len, segment.len);
		write32(&reg_addr->geni_tx_watermark_reg, TX_WATERMARK);
		dout = segment.buf;
	} else {
		write32(&reg_addr->rx_trans_len.i2c_rx_trans_len, segment.len);
		din = segment.buf;
	}

	master_cmd_reg_val |= (prams & M_PARAMS_MSK);
	write32(&reg_addr->geni_m_cmd0, master_cmd_reg_val);

	return qup_handle_transfer(reg_addr, dout, din, segment.len);
}

static int i2c_transfer(struct I2cOps *me, I2cSeg *segments, int seg_count)
{
	QupI2c *bus = container_of(me, QupI2c, ops);
	I2cSeg *seg = segments;
	int ret = 0;

	while (!ret && seg_count--) {
		/* If stretch bit is set to 1 master will send repeated start
		 * condition to slave to continue the transfer, otherwise if
		 * it's set to 0 master will send stop bit to slave to stop
		 * the transfer
		 */
		u32 stretch = (seg_count ? 1 : 0);
		u32 m_param = 0;

		m_param |= (stretch << 2);
		m_param |= ((seg->chip & 0x7F) << 9);
		ret = i2c_do_xfer(bus->reg_addr, *seg, m_param);
		seg++;
	}
	return ret;
}

QupI2c *new_Qup_i2c(uintptr_t regs)
{
	QupI2c *bus = xzalloc(sizeof(*bus));

	bus->reg_addr = (QupRegs *)regs;
	bus->ops.transfer = &i2c_transfer;
	return bus;
}
