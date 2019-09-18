/*
 * This file is part of the coreboot project.
 *
 * Copyright (c) 2018-2019 Qualcomm Technologies
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

#include "drivers/soc/qcom_qup_se.h"

u32 qup_wait_for_irq(QupRegs *regs)
{
	struct stopwatch sw;
	unsigned int m_irq = 0;

	stopwatch_init_usecs_expire(&sw, 25);
	while (!stopwatch_expired(&sw)) {
		m_irq = read32(&regs->geni_m_irq_status);
		if (m_irq)
			break;
	}
	return m_irq;
}

static int handle_tx(QupRegs *regs, const u8 *dout, unsigned int tx_rem_bytes)
{
	int max_bytes = 0;

	max_bytes = (FIFO_DEPTH - TX_WATERMARK) * BYTES_PER_FIFO_WORD;
	max_bytes = MIN(tx_rem_bytes, max_bytes);

	buffer_to_fifo32((void *)dout, max_bytes, &regs->geni_tx_fifon,
					0, BYTES_PER_FIFO_WORD);

	if (tx_rem_bytes == max_bytes)
		write32(&regs->geni_tx_watermark_reg, 0);
	return max_bytes;
}

static int handle_rx(QupRegs *regs, u8 *din, unsigned int rx_rem_bytes)
{
	u32 rx_fifo_status = read32(&regs->geni_rx_fifo_status);
	int rx_bytes = 0;

	rx_bytes = (rx_fifo_status & RX_FIFO_WC_MSK) * BYTES_PER_FIFO_WORD;
	rx_bytes = MIN(rx_rem_bytes, rx_bytes);

	buffer_from_fifo32(din, rx_bytes, &regs->geni_rx_fifon,
					 0, BYTES_PER_FIFO_WORD);

	return rx_bytes;
}

void qup_handle_error(QupRegs *regs)
{
	u32 m_irq;
	struct stopwatch sw;

	write32(&regs->geni_tx_watermark_reg, 0);
	write32(&regs->geni_m_cmd_ctrl_reg, M_GENI_CMD_CANCEL);

	stopwatch_init_usecs_expire(&sw, 100);
	do {
		m_irq = qup_wait_for_irq(regs);
		if (m_irq & M_CMD_CANCEL_EN) {
			write32(&regs->geni_m_irq_clear, m_irq);
			break;
		}
		write32(&regs->geni_m_irq_clear, m_irq);
	} while (!stopwatch_expired(&sw));

	if (!(m_irq & M_CMD_CANCEL_EN)) {
		printf("%s:Cancel failed, Abort the operation\n", __func__);
		write32(&regs->geni_m_cmd_ctrl_reg, M_GENI_CMD_ABORT);

		stopwatch_init_usecs_expire(&sw, 100);
		do {
			m_irq = qup_wait_for_irq(regs);
			if (m_irq & M_CMD_CANCEL_EN) {
				write32(&regs->geni_m_irq_clear, m_irq);
				break;
			}
			write32(&regs->geni_m_irq_clear, m_irq);
		} while (!stopwatch_expired(&sw));

		if (!(m_irq & M_CMD_ABORT_EN))
			printf("%s:Abort failed\n", __func__);
	}
}

int qup_handle_transfer(QupRegs *regs, const void *dout, void *din, int size)
{
	unsigned int m_irq;
	struct stopwatch sw;
	unsigned int rx_rem_bytes = din ? size : 0;
	unsigned int tx_rem_bytes = dout ? size : 0;

	stopwatch_init_msecs_expire(&sw, 1000);
	do {
		m_irq = qup_wait_for_irq(regs);
		if ((m_irq & M_RX_FIFO_WATERMARK_EN) ||
					(m_irq & M_RX_FIFO_LAST_EN))
			rx_rem_bytes -= handle_rx(regs, din + size
						- rx_rem_bytes, rx_rem_bytes);
		if (m_irq & M_TX_FIFO_WATERMARK_EN)
			tx_rem_bytes -= handle_tx(regs, dout + size
						- tx_rem_bytes, tx_rem_bytes);
		if (m_irq & M_CMD_DONE_EN) {
			write32(&regs->geni_m_irq_clear, m_irq);
			break;
		}
		write32(&regs->geni_m_irq_clear, m_irq);
	} while (!stopwatch_expired(&sw));

	if (!(m_irq & M_CMD_DONE_EN) || tx_rem_bytes || rx_rem_bytes) {
		printf("%s:Error: Transfer failed\n", __func__);
		qup_handle_error(regs);
		return -1;
	}
	return 0;
}
