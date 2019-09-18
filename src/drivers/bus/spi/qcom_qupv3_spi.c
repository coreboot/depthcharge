/*
 * This file is part of the coreboot project.
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

#include "drivers/bus/spi/qcom_qupv3_spi.h"

/* SE_SPI_LOOPBACK register fields */
#define LOOPBACK_ENABLE	0x1

/* SE_SPI_WORD_LEN register fields */
#define WORD_LEN_MSK	GENMASK(9, 0)
#define MIN_WORD_LEN	4

/* SPI_TX/SPI_RX_TRANS_LEN fields */
#define TRANS_LEN_MSK	GENMASK(23, 0)

/* M_CMD OP codes for SPI */
#define SPI_TX_ONLY	1
#define SPI_RX_ONLY	2
#define SPI_FULL_DUPLEX	3
#define SPI_TX_RX	7
#define SPI_CS_ASSERT	8
#define SPI_CS_DEASSERT	9
#define SPI_SCK_ONLY	10

/* M_CMD params for SPI */
/* If fragmentation bit is set then CS will not toggle after each transfer */
#define M_CMD_FRAGMENTATION	BIT(2)

static void setup_fifo_params(QupRegs *regs)
{
	u32 word_len = 0;

	/* Disable loopback mode */
	write32(&regs->proto_loopback_cfg, 0);

	/* Always toggle CS0 */
	write32(&regs->spi_demux_sel, 0);
	word_len = ((BITS_PER_WORD - MIN_WORD_LEN) & WORD_LEN_MSK);
	write32(&regs->word_len.spi_word_len, word_len);

	/* FIFO PACKING CONFIGURATION */
	write32(&regs->geni_tx_packing_cfg0, PACK_VECTOR0
						| (PACK_VECTOR1 << 10));
	write32(&regs->geni_tx_packing_cfg1, PACK_VECTOR2
						| (PACK_VECTOR3 << 10));
	write32(&regs->geni_rx_packing_cfg0, PACK_VECTOR0
						| (PACK_VECTOR1 << 10));
	write32(&regs->geni_rx_packing_cfg1, PACK_VECTOR2
						| (PACK_VECTOR3 << 10));
	write32(&regs->geni_byte_granularity, (log2(BITS_PER_WORD) - 3));
}

static void qup_setup_m_cmd(QupRegs *regs, u32 cmd, u32 params)
{
	u32 m_cmd = (cmd << M_OPCODE_SHFT);

	m_cmd |= (params & M_PARAMS_MSK);
	write32(&regs->geni_m_cmd0, m_cmd);
}

static int qup_spi_xfer(SpiOps *me, void *in, const void *out, uint32_t size)
{
	Sc7180QupSpi *bus = container_of(me, Sc7180QupSpi, ops);
	u32 m_cmd = 0;
	u32 m_param = M_CMD_FRAGMENTATION;
	QupRegs *regs = bus->reg_addr;

	if (size == 0)
		return 0;

	setup_fifo_params(regs);

	if (!out)
		m_cmd = SPI_RX_ONLY;
	else if (!in)
		m_cmd = SPI_TX_ONLY;
	else
		m_cmd = SPI_FULL_DUPLEX;

	/* Check for maximum permissible transfer length */
	assert(!(size & ~TRANS_LEN_MSK));

	if (out) {
		write32(&regs->tx_trans_len.spi_tx_trans_len, size);
		write32(&regs->geni_tx_watermark_reg, TX_WATERMARK);
	}
	if (in)
		write32(&regs->rx_trans_len.spi_rx_trans_len, size);

	qup_setup_m_cmd(regs, m_cmd, m_param);

	if (qup_handle_transfer(regs, out, in, size))
		return -1;
	return 0;
}

static int spi_qup_set_cs(QupRegs *regs, bool enable)
{
	u32 m_cmd = 0, m_irq;
	struct stopwatch sw;

	m_cmd = (enable) ? SPI_CS_ASSERT : SPI_CS_DEASSERT;
	qup_setup_m_cmd(regs, m_cmd, 0);

	stopwatch_init_usecs_expire(&sw, 100);
	do {
		m_irq = qup_wait_for_irq(regs);
		if (m_irq & M_CMD_DONE_EN) {
			write32(&regs->geni_m_irq_clear, m_irq);
			break;
		}
		write32(&regs->geni_m_irq_clear, m_irq);
	} while (!stopwatch_expired(&sw));

	if (!(m_irq & M_CMD_DONE_EN)) {
		printf("%s:Failed to %s chip\n", __func__,
					(enable) ? "Assert" : "Deassert");
		qup_handle_error(regs);
		return -1;
	}
	return 0;
}

static int qup_spi_start_bus(SpiOps *me)
{
	Sc7180QupSpi *bus = container_of(me, Sc7180QupSpi, ops);
	QupRegs *regs = bus->reg_addr;

	return spi_qup_set_cs(regs, 1);
}

static int qup_spi_stop_bus(SpiOps *me)
{
	Sc7180QupSpi *bus = container_of(me, Sc7180QupSpi, ops);
	QupRegs *regs = bus->reg_addr;

	return spi_qup_set_cs(regs, 0);
}

Sc7180QupSpi *new_sc7180_Qup_spi(uintptr_t reg_addr)
{
	Sc7180QupSpi *bus = xzalloc(sizeof(*bus));

	bus->reg_addr = (QupRegs *)reg_addr;
	bus->ops.start = &qup_spi_start_bus;
	bus->ops.stop = &qup_spi_stop_bus;
	bus->ops.transfer = &qup_spi_xfer;
	return bus;
}

