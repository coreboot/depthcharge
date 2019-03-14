/*
 * Copyright (c) 2012, 2016, 2019 The Linux Foundation. All rights reserved.
 * Copyright 2014 Google Inc.
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

#ifndef _QCS405_SPI_H_
#define _QCS405_SPI_H_

#include "drivers/flash/spi.h"
#include "drivers/bus/spi/spi.h"

#define QUP0_BASE	((unsigned char *)0x7AF5000u)
#define QUP1_BASE	((unsigned char *)0x78b6000u)
#define QUP2_BASE	((unsigned char *)0x78b7000u)
#define QUP3_BASE	((unsigned char *)0x78b8000u)
#define QUP4_BASE	((unsigned char *)0x78b9000u)

#define BLSP_QUP0_REG_BASE		(QUP0_BASE + 0x00000000)
#define BLSP_QUP1_REG_BASE		(QUP1_BASE + 0x00000000)
#define BLSP_QUP2_REG_BASE		(QUP2_BASE + 0x00000000)
#define BLSP_QUP3_REG_BASE		(QUP3_BASE + 0x00000000)
#define BLSP_QUP4_REG_BASE		(QUP4_BASE + 0x00000000)

#define BLSP0_SPI_CONFIG_REG		(BLSP_QUP0_REG_BASE + 0x00000300)
#define BLSP1_SPI_CONFIG_REG		(BLSP_QUP1_REG_BASE + 0x00000300)
#define BLSP4_SPI_CONFIG_REG		(BLSP_QUP4_REG_BASE + 0x00000300)
#define BLSP5_SPI_CONFIG_REG		(BLSP_QUP0_REG_BASE + 0x00000300)

#define BLSP0_SPI_IO_CONTROL_REG	(BLSP_QUP0_REG_BASE + 0x00000304)
#define BLSP1_SPI_IO_CONTROL_REG	(BLSP_QUP1_REG_BASE + 0x00000304)
#define BLSP4_SPI_IO_CONTROL_REG	(BLSP_QUP4_REG_BASE + 0x00000304)
#define BLSP5_SPI_IO_CONTROL_REG	(BLSP_QUP0_REG_BASE + 0x00000304)

#define BLSP0_SPI_ERROR_FLAGS_REG	(BLSP_QUP0_REG_BASE + 0x00000308)
#define BLSP1_SPI_ERROR_FLAGS_REG	(BLSP_QUP1_REG_BASE + 0x00000308)
#define BLSP4_SPI_ERROR_FLAGS_REG	(BLSP_QUP4_REG_BASE + 0x00000308)
#define BLSP5_SPI_ERROR_FLAGS_REG	(BLSP_QUP0_REG_BASE + 0x00000308)

#define BLSP0_SPI_DEASSERT_WAIT_REG	(BLSP_QUP0_REG_BASE + 0x00000310)
#define BLSP1_SPI_DEASSERT_WAIT_REG	(BLSP_QUP1_REG_BASE + 0x00000310)
#define BLSP4_SPI_DEASSERT_WAIT_REG	(BLSP_QUP4_REG_BASE + 0x00000310)
#define BLSP5_SPI_DEASSERT_WAIT_REG	(BLSP_QUP0_REG_BASE + 0x00000310)
#define BLSP0_SPI_ERROR_FLAGS_EN_REG	(BLSP_QUP0_REG_BASE + 0x0000030c)
#define BLSP1_SPI_ERROR_FLAGS_EN_REG	(BLSP_QUP1_REG_BASE + 0x0000030c)
#define BLSP4_SPI_ERROR_FLAGS_EN_REG	(BLSP_QUP4_REG_BASE + 0x0000030c)
#define BLSP5_SPI_ERROR_FLAGS_EN_REG	(BLSP_QUP0_REG_BASE + 0x0000030c)

#define BLSP_QUP0_CONFIG_REG		(BLSP_QUP0_REG_BASE + 0x00000000)
#define BLSP_QUP1_CONFIG_REG		(BLSP_QUP1_REG_BASE + 0x00000000)
#define BLSP_QUP4_CONFIG_REG		(BLSP_QUP4_REG_BASE + 0x00000000)

#define BLSP_QUP0_ERROR_FLAGS_REG	(BLSP_QUP0_REG_BASE      + 0x0000001c)
#define BLSP_QUP1_ERROR_FLAGS_REG	(BLSP_QUP1_REG_BASE      + 0x0000001c)
#define BLSP_QUP4_ERROR_FLAGS_REG	(BLSP_QUP4_REG_BASE      + 0x0000001c)

#define BLSP_QUP0_ERROR_FLAGS_EN_REG	(BLSP_QUP0_REG_BASE + 0x00000020)
#define BLSP_QUP1_ERROR_FLAGS_EN_REG	(BLSP_QUP1_REG_BASE + 0x00000020)
#define BLSP_QUP4_ERROR_FLAGS_EN_REG	(BLSP_QUP4_REG_BASE + 0x00000020)

#define BLSP_QUP0_OPERATIONAL_MASK	(BLSP_QUP0_REG_BASE + 0x00000028)
#define BLSP_QUP1_OPERATIONAL_MASK	(BLSP_QUP1_REG_BASE + 0x00000028)
#define BLSP_QUP4_OPERATIONAL_MASK	(BLSP_QUP4_REG_BASE + 0x00000028)

#define BLSP_QUP0_OPERATIONAL_REG	(BLSP_QUP0_REG_BASE + 0x00000018)
#define BLSP_QUP1_OPERATIONAL_REG	(BLSP_QUP1_REG_BASE + 0x00000018)
#define BLSP_QUP4_OPERATIONAL_REG	(BLSP_QUP4_REG_BASE + 0x00000018)

#define BLSP_QUP0_IO_MODES_REG		(BLSP_QUP0_REG_BASE + 0x00000008)
#define BLSP_QUP1_IO_MODES_REG		(BLSP_QUP1_REG_BASE + 0x00000008)
#define BLSP_QUP4_IO_MODES_REG		(BLSP_QUP4_REG_BASE + 0x00000008)

#define BLSP_QUP0_STATE_REG		(BLSP_QUP0_REG_BASE + 0x00000004)
#define BLSP_QUP1_STATE_REG		(BLSP_QUP1_REG_BASE + 0x00000004)
#define BLSP_QUP4_STATE_REG		(BLSP_QUP4_REG_BASE + 0x00000004)

#define BLSP_QUP0_INPUT_FIFOc_REG(c) \
		(BLSP_QUP0_REG_BASE + 0x00000218 + 4 * (c))
#define BLSP_QUP1_INPUT_FIFOc_REG(c) \
		(BLSP_QUP1_REG_BASE + 0x00000218 + 4 * (c))
#define BLSP_QUP4_INPUT_FIFOc_REG(c) \
		(BLSP_QUP4_REG_BASE + 0x00000218 + 4 * (c))

#define BLSP_QUP0_OUTPUT_FIFOc_REG(c) \
		(BLSP_QUP0_REG_BASE + 0x00000110 + 4 * (c))
#define BLSP_QUP1_OUTPUT_FIFOc_REG(c) \
		(BLSP_QUP1_REG_BASE + 0x00000110 + 4 * (c))
#define BLSP_QUP4_OUTPUT_FIFOc_REG(c) \
		(BLSP_QUP4_REG_BASE + 0x00000110 + 4 * (c))

#define BLSP_QUP0_MX_INPUT_COUNT_REG	(BLSP_QUP0_REG_BASE + 0x00000200)
#define BLSP_QUP1_MX_INPUT_COUNT_REG	(BLSP_QUP1_REG_BASE + 0x00000200)
#define BLSP_QUP4_MX_INPUT_COUNT_REG	(BLSP_QUP4_REG_BASE + 0x00000200)

#define BLSP_QUP0_MX_OUTPUT_COUNT_REG	(BLSP_QUP0_REG_BASE + 0x00000100)
#define BLSP_QUP1_MX_OUTPUT_COUNT_REG	(BLSP_QUP1_REG_BASE + 0x00000100)
#define BLSP_QUP4_MX_OUTPUT_COUNT_REG	(BLSP_QUP4_REG_BASE + 0x00000100)

#define BLSP_QUP0_SW_RESET_REG		(BLSP_QUP0_REG_BASE + 0x0000000c)
#define BLSP_QUP1_SW_RESET_REG		(BLSP_QUP1_REG_BASE + 0x0000000c)
#define BLSP_QUP4_SW_RESET_REG		(BLSP_QUP4_REG_BASE + 0x0000000c)

#define QUP_STATE_VALID_BIT			2
#define QUP_STATE_VALID				1
#define QUP_STATE_MASK				0x3
#define QUP_CONFIG_MINI_CORE_MSK		(0x0F << 8)
#define QUP_CONFIG_MINI_CORE_SPI		(1 << 8)
#define QUP_CONFIG_MINI_CORE_I2C		(2 << 8)
#define QUP_CONF_INPUT_MSK			(1 << 7)
#define QUP_CONF_INPUT_ENA			(0 << 7)
#define QUP_CONF_NO_INPUT			(1 << 7)
#define QUP_CONF_OUTPUT_MSK			(1 << 6)
#define QUP_CONF_OUTPUT_ENA			(0 << 6)
#define QUP_CONF_NO_OUTPUT			(1 << 6)
#define QUP_STATE_RUN_STATE			0x1
#define QUP_STATE_RESET_STATE			0x0
#define QUP_STATE_PAUSE_STATE			0x3
#define SPI_BIT_WORD_MSK			0x1F
#define SPI_8_BIT_WORD				0x07
#define LOOP_BACK_MSK				(1 << 8)
#define NO_LOOP_BACK				(0 << 8)
#define SLAVE_OPERATION_MSK			(1 << 5)
#define SLAVE_OPERATION				(0 << 5)
#define CLK_ALWAYS_ON				(0 << 9)
#define MX_CS_MODE				(1 << 8)
#define NO_TRI_STATE				(1 << 0)
#define FORCE_CS_MSK				(1 << 11)
#define FORCE_CS_EN				(1 << 11)
#define FORCE_CS_DIS				(0 << 11)
#define OUTPUT_BIT_SHIFT_MSK			(1 << 16)
#define OUTPUT_BIT_SHIFT_EN			(1 << 16)
#define INPUT_BLOCK_MODE_MSK			(0x03 << 12)
#define INPUT_BLOCK_MODE			(0x01 << 12)
#define OUTPUT_BLOCK_MODE_MSK			(0x03 << 10)
#define OUTPUT_BLOCK_MODE			(0x01 << 10)

#define SPI_INPUT_FIRST_MODE			(1 << 9)
#define SPI_IO_CONTROL_CLOCK_IDLE_HIGH		(1 << 10)
#define QUP_DATA_AVAILABLE_FOR_READ		(1 << 5)
#define QUP_OUTPUT_FIFO_NOT_EMPTY		(1 << 4)
#define OUTPUT_SERVICE_FLAG			(1 << 8)
#define INPUT_SERVICE_FLAG			(1 << 9)
#define QUP_OUTPUT_FIFO_FULL			(1 << 6)
#define MAX_OUTPUT_DONE_FLAG		(1<<10)
#define MAX_INPUT_DONE_FLAG		(1<<11)
#define SPI_INPUT_BLOCK_SIZE			4
#define SPI_OUTPUT_BLOCK_SIZE			4
#define MSM_QUP_MAX_FREQ			51200000
#define MAX_COUNT_SIZE				0xffff

#define SPI_RESET_STATE				0
#define SPI_RUN_STATE				1
#define SPI_CORE_RESET				0
#define SPI_CORE_RUNNING			1
#define SPI_MODE0				0
#define SPI_MODE1				1
#define SPI_MODE2				2
#define SPI_MODE3				3
#define BLSP0_SPI				0
#define BLSP1_SPI				1
#define BLSP4_SPI				4
#define BLSP5_SPI				5

struct blsp_spi {
	void *spi_config;
	void *io_control;
	void *error_flags;
	void *error_flags_en;
	void *qup_config;
	void *qup_error_flags;
	void *qup_error_flags_en;
	void *qup_operational;
	void *qup_io_modes;
	void *qup_state;
	void *qup_input_fifo;
	void *qup_output_fifo;
	void *qup_mx_input_count;
	void *qup_mx_output_count;
	void *qup_sw_reset;
	void *qup_ns_reg;
	void *qup_md_reg;
	void *qup_op_mask;
	void *qup_deassert_wait;
};

static const struct blsp_spi spi_reg[] = {
	/* BLSP0 registers for SPI interface */
	{
		BLSP0_SPI_CONFIG_REG,
		BLSP0_SPI_IO_CONTROL_REG,
		BLSP0_SPI_ERROR_FLAGS_REG,
		BLSP0_SPI_ERROR_FLAGS_EN_REG,
		BLSP_QUP0_CONFIG_REG,
		BLSP_QUP0_ERROR_FLAGS_REG,
		BLSP_QUP0_ERROR_FLAGS_EN_REG,
		BLSP_QUP0_OPERATIONAL_REG,
		BLSP_QUP0_IO_MODES_REG,
		BLSP_QUP0_STATE_REG,
		BLSP_QUP0_INPUT_FIFOc_REG(0),
		BLSP_QUP0_OUTPUT_FIFOc_REG(0),
		BLSP_QUP0_MX_INPUT_COUNT_REG,
		BLSP_QUP0_MX_OUTPUT_COUNT_REG,
		BLSP_QUP0_SW_RESET_REG,
		0,
		0,
		BLSP_QUP0_OPERATIONAL_MASK,
		BLSP0_SPI_DEASSERT_WAIT_REG,
	},
	/* BLSP1 registers for SPI interface */
	{
		BLSP1_SPI_CONFIG_REG,
		BLSP1_SPI_IO_CONTROL_REG,
		BLSP1_SPI_ERROR_FLAGS_REG,
		BLSP1_SPI_ERROR_FLAGS_EN_REG,
		BLSP_QUP1_CONFIG_REG,
		BLSP_QUP1_ERROR_FLAGS_REG,
		BLSP_QUP1_ERROR_FLAGS_EN_REG,
		BLSP_QUP1_OPERATIONAL_REG,
		BLSP_QUP1_IO_MODES_REG,
		BLSP_QUP1_STATE_REG,
		BLSP_QUP1_INPUT_FIFOc_REG(0),
		BLSP_QUP1_OUTPUT_FIFOc_REG(0),
		BLSP_QUP1_MX_INPUT_COUNT_REG,
		BLSP_QUP1_MX_OUTPUT_COUNT_REG,
		BLSP_QUP1_SW_RESET_REG,
		0,
		0,
		BLSP_QUP0_OPERATIONAL_MASK,
		BLSP0_SPI_DEASSERT_WAIT_REG,
	},{0},{0},

	/* BLSP4 registers for SPI interface */
	{
		BLSP4_SPI_CONFIG_REG,
		BLSP4_SPI_IO_CONTROL_REG,
		BLSP4_SPI_ERROR_FLAGS_REG,
		BLSP4_SPI_ERROR_FLAGS_EN_REG,
		BLSP_QUP4_CONFIG_REG,
		BLSP_QUP4_ERROR_FLAGS_REG,
		BLSP_QUP4_ERROR_FLAGS_EN_REG,
		BLSP_QUP4_OPERATIONAL_REG,
		BLSP_QUP4_IO_MODES_REG,
		BLSP_QUP4_STATE_REG,
		BLSP_QUP4_INPUT_FIFOc_REG(0),
		BLSP_QUP4_OUTPUT_FIFOc_REG(0),
		BLSP_QUP4_MX_INPUT_COUNT_REG,
		BLSP_QUP4_MX_OUTPUT_COUNT_REG,
		BLSP_QUP4_SW_RESET_REG,
		0,
		0,
		BLSP_QUP4_OPERATIONAL_MASK,
		BLSP4_SPI_DEASSERT_WAIT_REG,
	},

	/* BLSP5 registers for SPI interface */
	{
		BLSP5_SPI_CONFIG_REG,
		BLSP5_SPI_IO_CONTROL_REG,
		BLSP5_SPI_ERROR_FLAGS_REG,
		BLSP5_SPI_ERROR_FLAGS_EN_REG,
		BLSP_QUP0_CONFIG_REG,
		BLSP_QUP0_ERROR_FLAGS_REG,
		BLSP_QUP0_ERROR_FLAGS_EN_REG,
		BLSP_QUP0_OPERATIONAL_REG,
		BLSP_QUP0_IO_MODES_REG,
		BLSP_QUP0_STATE_REG,
		BLSP_QUP0_INPUT_FIFOc_REG(0),
		BLSP_QUP0_OUTPUT_FIFOc_REG(0),
		BLSP_QUP0_MX_INPUT_COUNT_REG,
		BLSP_QUP0_MX_OUTPUT_COUNT_REG,
		BLSP_QUP0_SW_RESET_REG,
		0,
		0,
		BLSP_QUP0_OPERATIONAL_MASK,
		BLSP5_SPI_DEASSERT_WAIT_REG,
	},
};

#define SUCCESS		0

#define DUMMY_DATA_VAL		0
#define TIMEOUT_CNT		100

/* MX_INPUT_COUNT and MX_OUTPUT_COUNT are 16-bits. Zero has a special meaning
 * (count function disabled) and does not hold significance in the count. */
#define MAX_PACKET_COUNT	((64 * KiB) - 1)


struct spi_slave {
	unsigned int	bus;
	unsigned int	cs;
	unsigned int	rw;
};

struct qcs_spi_slave {
	struct spi_slave slave;
	const struct blsp_spi *regs;
	unsigned int core_state;
	unsigned int mode;
	unsigned int initialized;
	unsigned long freq;
	int allocated;
};

typedef struct {
	SpiOps ops;
	struct qcs_spi_slave qcs_spi_slave;
} SpiController;

static inline struct qcs_spi_slave *to_qcs_spi(SpiOps *spi_ops)
{
	SpiController *controller;

	controller = container_of(spi_ops, SpiController, ops);

	return &controller->qcs_spi_slave;
}
SpiController *new_spi(unsigned int bus_num, unsigned int cs);

#endif /* _QCS405_SPI_H_ */
