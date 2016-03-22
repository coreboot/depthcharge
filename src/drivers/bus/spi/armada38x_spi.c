/*
 * Copyright (c) 2012 The Linux Foundation. All rights reserved.
 * Copyright 2015 Marvell Inc.
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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libpayload.h>
#include <assert.h>
#include "base/container_of.h"
#include "armada38x_spi.h"

#define MV_SPI_IF_CONFIG_REG(bus) ((bus * 0x80) + 0x04)
#define MV_SPI_SPR_OFFSET 0
#define MV_SPI_SPR_MASK (0xF << MV_SPI_SPR_OFFSET)
#define MV_SPI_SPPR_0_OFFSET 4
#define MV_SPI_SPPR_0_MASK (0x1 << MV_SPI_SPPR_0_OFFSET)
#define MV_SPI_SPPR_HI_OFFSET 6
#define MV_SPI_SPPR_HI_MASK (0x3 << MV_SPI_SPPR_HI_OFFSET)

#define MV_SPI_BYTE_LENGTH_OFFSET 5 /* bit 5 */
#define MV_SPI_BYTE_LENGTH_MASK (0x1 << MV_SPI_BYTE_LENGTH_OFFSET)

#define MV_SPI_IF_CTRL_REG(bus) ((bus * 0x80) + 0x00)
#define MV_SPI_CS_ENABLE_OFFSET 0 /* bit 0 */
#define MV_SPI_CS_ENABLE_MASK (0x1 << MV_SPI_CS_ENABLE_OFFSET)

#define MV_SPI_TMNG_PARAMS_REG(bus) ((bus * 0x80) + 0x18)
#define MV_SPI_TMISO_SAMPLE_OFFSET 6
#define MV_SPI_TMISO_SAMPLE_MASK (0x3 << MV_SPI_TMISO_SAMPLE_OFFSET)

typedef enum {
	SPI_TYPE_FLASH = 0,
	SPI_TYPE_SLIC_ZARLINK_SILABS,
	SPI_TYPE_SLIC_LANTIQ,
	SPI_TYPE_SLIC_ZSI,
	SPI_TYPE_SLIC_ISI
} MV_SPI_TYPE;

#define MV_SPI_CS_NUM_OFFSET 2
#define MV_SPI_CS_NUM_MASK (0x7 << MV_SPI_CS_NUM_OFFSET)
#define MV_SPI_CPOL_OFFSET 11
#define MV_SPI_CPOL_MASK (0x1 << MV_SPI_CPOL_OFFSET)
#define MV_SPI_CPHA_OFFSET 12
#define MV_SPI_CPHA_MASK (0x1 << MV_SPI_CPHA_OFFSET)
#define MV_SPI_TXLSBF_OFFSET 13
#define MV_SPI_TXLSBF_MASK (0x1 << MV_SPI_TXLSBF_OFFSET)
#define MV_SPI_RXLSBF_OFFSET 14
#define MV_SPI_RXLSBF_MASK (0x1 << MV_SPI_RXLSBF_OFFSET)

#define MV_SPI_INT_CAUSE_REG(bus) ((bus * 0x80) + 0x10)
#define MV_SPI_DATA_OUT_REG(bus) ((bus * 0x80) + 0x08)
#define MV_SPI_WAIT_RDY_MAX_LOOP 100000
#define MV_SPI_DATA_IN_REG(bus) ((bus * 0x80) + 0x0c)

/******************************************************************************
struct define
*******************************************************************************/
typedef struct {
	int clock_pol_low;
	enum { SPI_CLK_HALF_CYC, SPI_CLK_BEGIN_CYC } clock_phase;
	int tx_msb_first;
	int rx_msb_first;
} MV_SPI_IF_PARAMS;

typedef struct {
	/* Does this device support 16 bits access */
	int en16_bit;
	/* should we assert / disassert CS for each byte we read / write */
	int byte_cs_asrt;
	int clock_pol_low;
	unsigned int baud_rate;
	unsigned int clk_phase;
} MV_SPI_TYPE_INFO;
/******************************************************************************
struct define end
*******************************************************************************/

/******************************************************************************
param define
*******************************************************************************/
static MV_SPI_TYPE_INFO spi_types[] = { {.en16_bit = 1,
				       .clock_pol_low = 1,
				       .byte_cs_asrt = 0,
				       .baud_rate = (20 << 20), /*  20M */
				       .clk_phase = SPI_CLK_BEGIN_CYC},
				      {.en16_bit = 0,
				       .clock_pol_low = 1,
				       .byte_cs_asrt = 1,
				       .baud_rate = 0x00800000,
				       .clk_phase = SPI_CLK_BEGIN_CYC},
				      {.en16_bit = 0,
				       .clock_pol_low = 1,
				       .byte_cs_asrt = 0,
				       .baud_rate = 0x00800000,
				       .clk_phase = SPI_CLK_BEGIN_CYC},
				      {.en16_bit = 0,
				       .clock_pol_low = 1,
				       .byte_cs_asrt = 1,
				       .baud_rate = 0x00800000,
				       .clk_phase = SPI_CLK_HALF_CYC},
				      {.en16_bit = 0,
				       .clock_pol_low = 0,
				       .byte_cs_asrt = 1,
				       .baud_rate = 0x00200000,
				       .clk_phase = SPI_CLK_HALF_CYC} };

/******************************************************************************
param define end
*******************************************************************************/
static inline uint32_t spi_reg_read(uint32_t reg)
{
	return read32((void *)reg);
}
static inline void spi_reg_write(uint32_t reg, uint32_t val)
{
	write32((void *)reg, val);
}
static inline void spi_reg_bit_set(uint32_t reg, uint32_t bitMask)
{
	spi_reg_write(reg, (spi_reg_read(reg) | bitMask));
}
static inline void spi_reg_bit_reset(uint32_t reg, uint32_t bitMask)
{
	spi_reg_write(reg, (spi_reg_read(reg) & (~bitMask)));
}

static int mv_spi_baud_rate_set(SpiController *spi)
{
	unsigned int spr, sppr;
	unsigned int divider;
	unsigned int best_spr = 0, best_sppr = 0;
	unsigned char exact_match = 0;
	unsigned int min_baud_offset = 0xFFFFFFFF;
	unsigned int temp_reg;
	unsigned int base, bus, cpu_clk, serial_baud_rate;

	base = spi->armada38x_spi_slave.base;
	bus = spi->armada38x_spi_slave.slave.bus;
	cpu_clk = spi->armada38x_spi_slave.tclk;
	serial_baud_rate = spi->armada38x_spi_slave.max_hz;
	assert(cpu_clk != serial_baud_rate);
	/* Find the best prescale configuration - less or equal */
	for (spr = 1; spr <= 15; spr++) {
		for (sppr = 0; sppr <= 7; sppr++) {
			divider = spr * (1 << sppr);
			/* check for higher - irrelevant */
			if ((cpu_clk / divider) > serial_baud_rate)
				continue;

			/* check for exact fit */
			if ((cpu_clk / divider) == serial_baud_rate) {
				best_spr = spr;
				best_sppr = sppr;
				exact_match = 1;
				break;
			}

			/* check if this is better than the previous one */
			if ((serial_baud_rate - (cpu_clk / divider)) <
			    min_baud_offset) {
				min_baud_offset =
				    serial_baud_rate - cpu_clk / divider;
				best_spr = spr;
				best_sppr = sppr;
			}
		}

		if (exact_match == 1)
			break;
	}

	if (best_spr == 0) {
		printf("%s ERROR: SPI baud rate prescale error!\n",
		       __func__);
		return -1;
	}

	/* configure the Prescale */
	temp_reg = spi_reg_read(base + MV_SPI_IF_CONFIG_REG(bus)) &
		  ~(MV_SPI_SPR_MASK | MV_SPI_SPPR_0_MASK | MV_SPI_SPPR_HI_MASK);
	temp_reg |= ((best_spr << MV_SPI_SPR_OFFSET) |
		    ((best_sppr & 0x1) << MV_SPI_SPPR_0_OFFSET) |
		    ((best_sppr >> 1) << MV_SPI_SPPR_HI_OFFSET));
	spi_reg_write(base + MV_SPI_IF_CONFIG_REG(bus), temp_reg);

	return 0;
}

static void mv_spi_cs_deassert(SpiController *spi)
{
	unsigned int base, bus;

	base = spi->armada38x_spi_slave.base;
	bus = spi->armada38x_spi_slave.slave.bus;
	spi_reg_bit_reset(base + MV_SPI_IF_CTRL_REG(bus),
			  MV_SPI_CS_ENABLE_MASK);
}

static int mv_spi_cs_set(SpiController *spi)
{
	unsigned int ctrl_reg;
	static unsigned char last_cs_id = 0xFF;
	static unsigned char last_bus = 0xFF;
	unsigned int base, bus, cs_id;

	base = spi->armada38x_spi_slave.base;
	bus = spi->armada38x_spi_slave.slave.bus;
	cs_id = spi->armada38x_spi_slave.slave.cs;
	if (cs_id > 7){
		printf("%s ERROR: invalid arg!\n", __func__);
		return -1;
	}

	if ((last_bus == bus) && (last_cs_id == cs_id))
		return 0;

	ctrl_reg = spi_reg_read(base + MV_SPI_IF_CTRL_REG(bus));
	ctrl_reg &= ~MV_SPI_CS_NUM_MASK;
	ctrl_reg |= (cs_id << MV_SPI_CS_NUM_OFFSET);
	spi_reg_write(base + MV_SPI_IF_CTRL_REG(bus), ctrl_reg);

	last_bus = bus;
	last_cs_id = cs_id;

	return 0;
}

static int mv_spi_if_config_set(SpiController *spi, MV_SPI_IF_PARAMS *if_params)
{
	unsigned int ctrl_reg;
	unsigned int base, bus;

	base = spi->armada38x_spi_slave.base;
	bus = spi->armada38x_spi_slave.slave.bus;
	ctrl_reg = spi_reg_read(base + MV_SPI_IF_CONFIG_REG(bus));

	/* Set Clock Polarity */
	ctrl_reg &= ~(MV_SPI_CPOL_MASK | MV_SPI_CPHA_MASK | MV_SPI_TXLSBF_MASK |
		     MV_SPI_RXLSBF_MASK);
	if (if_params->clock_pol_low)
		ctrl_reg |= MV_SPI_CPOL_MASK;

	if (if_params->clock_phase == SPI_CLK_BEGIN_CYC)
		ctrl_reg |= MV_SPI_CPHA_MASK;

	if (if_params->tx_msb_first)
		ctrl_reg |= MV_SPI_TXLSBF_MASK;

	if (if_params->rx_msb_first)
		ctrl_reg |= MV_SPI_RXLSBF_MASK;

	spi_reg_write(base + MV_SPI_IF_CONFIG_REG(bus), ctrl_reg);

	return 0;
}

static int mv_spi_params_set(SpiController *spi,
		MV_SPI_TYPE type)
{
	MV_SPI_IF_PARAMS if_params;

	if (0 != mv_spi_cs_set(spi)) {
		printf("Error, setting SPI CS failed\n");
		return -1;
	}

	if (spi->armada38x_spi_slave.type != type) {
		spi->armada38x_spi_slave.type = type;
		mv_spi_baud_rate_set(spi);

		if_params.clock_pol_low = spi_types[type].clock_pol_low;
		if_params.clock_phase = spi_types[type].clk_phase;
		if_params.tx_msb_first = 0;
		if_params.rx_msb_first = 0;
		mv_spi_if_config_set(spi, &if_params);
	}

	return 0;
}

static int mv_spi_init(SpiController *spi)
{
	int ret;
	unsigned int timing_reg;
	unsigned int base, bus;

	base = spi->armada38x_spi_slave.base;
	bus = spi->armada38x_spi_slave.slave.bus;
	/* Set the serial clock */
	ret = mv_spi_baud_rate_set(spi);
	if (ret != 0)
		return ret;

	/* Configure the default SPI mode to be 8bit */
	spi_reg_bit_reset(base + MV_SPI_IF_CONFIG_REG(bus),
			   MV_SPI_BYTE_LENGTH_MASK);

	timing_reg = spi_reg_read(base + MV_SPI_TMNG_PARAMS_REG(bus));
	timing_reg &= ~MV_SPI_TMISO_SAMPLE_MASK;
	timing_reg |= (0x2) << MV_SPI_TMISO_SAMPLE_OFFSET;
	spi_reg_write(base + MV_SPI_TMNG_PARAMS_REG(bus), timing_reg);

	/* Verify that the CS is deasserted */
	mv_spi_cs_deassert(spi);

	mv_spi_params_set(spi, SPI_TYPE_FLASH);

	return 0;
}

static void mv_spi_cs_assert(SpiController *spi)
{
	unsigned int base, bus;

	base = spi->armada38x_spi_slave.base;
	bus = spi->armada38x_spi_slave.slave.bus;
	spi_reg_bit_set(base + MV_SPI_IF_CTRL_REG(bus), MV_SPI_CS_ENABLE_MASK);
}

static int mv_spi_8bit_data_tx_rx(SpiController *spi,
		      unsigned char tx_data,
		      unsigned char *p_rx_data)
{
	unsigned int i;
	int ready = 0;
	unsigned int base, bus;
	MV_SPI_TYPE_INFO *spi_info = NULL;

	spi_info = &spi_types[spi->armada38x_spi_slave.type];
	base = spi->armada38x_spi_slave.base;
	bus = spi->armada38x_spi_slave.slave.bus;
	if (spi_info->byte_cs_asrt)
		mv_spi_cs_assert(spi);

	/* First clear the bit in the interrupt cause register */
	spi_reg_write(base + MV_SPI_INT_CAUSE_REG(bus), 0x0);

	/* Transmit data */
	spi_reg_write(base + MV_SPI_DATA_OUT_REG(bus), tx_data);

	/* wait with timeout for memory ready */
	for (i = 0; i < MV_SPI_WAIT_RDY_MAX_LOOP; i++) {
		if (spi_reg_read(base + MV_SPI_INT_CAUSE_REG(bus))) {
			ready = 1;
			break;
		}
	}

	if (!ready) {
		if (spi_info->byte_cs_asrt) {
			mv_spi_cs_deassert(spi);
			/* WA to compansate Zarlink SLIC CS off time */
			udelay(4);
		}
		printf("%s ERROR: timeout\n", __func__);
		return -1;
	}

	/* check that the RX data is needed */
	if (p_rx_data)
		*p_rx_data = spi_reg_read(base + MV_SPI_DATA_IN_REG(bus));

	if (spi_info->byte_cs_asrt) {
		mv_spi_cs_deassert(spi);
		/* WA to compansate Zarlink SLIC CS off time */
		udelay(4);
	}

	return 0;
}

static int mrvl_spi_xfer(SpiController *spi,
			 unsigned int bitlen,
			 const void *dout,
			 void *din)
{
	int ret;
	unsigned char *pdout = (unsigned char *)dout;
	unsigned char *pdin = (unsigned char *)din;
	int tmp_bitlen = bitlen;
	unsigned char tmp_dout = 0;
	unsigned int base, bus;

	base = spi->armada38x_spi_slave.base;
	bus = spi->armada38x_spi_slave.slave.bus;
	/* Verify that the SPI mode is in 8bit mode */
	spi_reg_bit_reset(base + MV_SPI_IF_CONFIG_REG(bus),
			 MV_SPI_BYTE_LENGTH_MASK);

	while (tmp_bitlen > 0) {
		if (pdout)
			tmp_dout = (*pdout) & 0xff;

		/* Transmitted and wait for the transfer to be completed */
		ret = mv_spi_8bit_data_tx_rx(spi, tmp_dout, pdin);
		if (ret != 0)
			return ret;

		/* increment the pointers */
		if (pdin)
			pdin++;
		if (pdout)
			pdout++;

		tmp_bitlen -= 8;
	}
	return 0;
}

static int spi_claim_bus(SpiOps *spi_ops)
{
	SpiController *spi = container_of(spi_ops, SpiController, ops);

	if (spi->armada38x_spi_slave.bus_started)
		return -1;

	mv_spi_cs_set(spi);
	mv_spi_cs_assert(spi);

	spi->armada38x_spi_slave.bus_started = 1;
	return 0;
}

static int spi_release_bus(SpiOps *spi_ops)
{
	SpiController *spi = container_of(spi_ops, SpiController, ops);

	mv_spi_cs_deassert(spi);
	spi->armada38x_spi_slave.bus_started = 0;
	return 0;
}

static int spi_xfer(SpiOps *spi_ops,
		    void *din,
		    const void *dout,
		    unsigned int bytes)
{
	int ret = -1;
	SpiController *spi = container_of(spi_ops, SpiController, ops);

	if (bytes)
		ret = mrvl_spi_xfer(spi, bytes * 8, dout, din);
	return ret;
}

SpiController *new_armada38x_spi(unsigned int base,
				unsigned int tclk,
				unsigned int bus_num,
				unsigned int cs,
				unsigned int max_hz)
{
	SpiController *bus = xzalloc(sizeof(*bus));

	bus->ops.start = spi_claim_bus;
	bus->ops.transfer = spi_xfer;
	bus->ops.stop = spi_release_bus;

	bus->armada38x_spi_slave.base = base;
	bus->armada38x_spi_slave.tclk = tclk;
	bus->armada38x_spi_slave.slave.bus = bus_num;
	bus->armada38x_spi_slave.slave.cs = cs;
	bus->armada38x_spi_slave.max_hz = max_hz;
	bus->armada38x_spi_slave.bus_started = 0;
	bus->armada38x_spi_slave.type = -1;

	mv_spi_init(bus);

	return bus;
}
