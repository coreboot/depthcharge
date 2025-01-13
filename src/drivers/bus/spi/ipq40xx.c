/*
 * Copyright (c) 2012, 2016 The Linux Foundation. All rights reserved.
 *
 * Copyright 2014 Google LLC
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
#include <libpayload.h>
#include "base/container_of.h"
#include "ipq40xx.h"
#include "drivers/storage/mtd/mtd.h"

static void write_force_cs(SpiOps *ops, int assert);


static int check_bit_state(void *reg_addr, int bit_num, int val, int us_delay)
{
	unsigned int count = TIMEOUT_CNT;
	unsigned int bit_val = ((read32(reg_addr) >> bit_num) & 0x01);

	while (bit_val != val) {
		count--;
		if (count == 0)
			return -ETIMEDOUT;
		udelay(us_delay);
		bit_val = ((read32(reg_addr) >> bit_num) & 0x01);
	}

	return SUCCESS;
}

/*
 * Check whether QUPn State is valid
 */
static int check_qup_state_valid(struct ipq_spi_slave *ds)
{

	return check_bit_state(ds->regs->qup_state, QUP_STATE_VALID_BIT,
				QUP_STATE_VALID, 1);

}

/*
 * Configure QUPn Core state
 */
static int config_spi_state(struct ipq_spi_slave *ds, unsigned int state)
{
	uint32_t val;
	int ret = SUCCESS;

	ret = check_qup_state_valid(ds);
	if (ret != SUCCESS)
		return ret;

	switch (state) {
	case SPI_RUN_STATE:
		/* Set the state to RUN */
		val = ((read32(ds->regs->qup_state) & ~QUP_STATE_MASK)
				| QUP_STATE_RUN_STATE);
		write32(ds->regs->qup_state, val);
		ret = check_qup_state_valid(ds);
		if (ret != SUCCESS)
			return ret;
		ds->core_state = SPI_CORE_RUNNING;
		break;
	case SPI_RESET_STATE:
		/* Set the state to RESET */
		val = ((read32(ds->regs->qup_state) & ~QUP_STATE_MASK)
				| QUP_STATE_RESET_STATE);
		write32(ds->regs->qup_state, val);
		ret = check_qup_state_valid(ds);
		if (ret != SUCCESS)
			return ret;
		ds->core_state = SPI_CORE_RESET;
		break;
	default:
		printf("unsupported QUP SPI state : %d\n", state);
		ret = -EINVAL;
		break;
	}

	return ret;
}

/*
 * Set QUPn SPI Mode
 */
static void spi_set_mode(struct ipq_spi_slave *ds, unsigned int mode)
{
	unsigned int clk_idle_state;
	unsigned int input_first_mode;
	uint32_t val;

	switch (mode) {
	case SPI_MODE0:
		clk_idle_state = 0;
		input_first_mode = SPI_INPUT_FIRST_MODE;
		break;
	case SPI_MODE1:
		clk_idle_state = 0;
		input_first_mode = 0;
		break;
	case SPI_MODE2:
		clk_idle_state = 1;
		input_first_mode = SPI_INPUT_FIRST_MODE;
		break;
	case SPI_MODE3:
		clk_idle_state = 1;
		input_first_mode = 0;
		break;
	default:
		printf("err : unsupported spi mode : %d\n", mode);
		return;
	}

	val = read32(ds->regs->spi_config);
	val |= input_first_mode;
	write32(ds->regs->spi_config, val);

	val = read32(ds->regs->io_control);
	if (clk_idle_state)
		val |= SPI_IO_CONTROL_CLOCK_IDLE_HIGH;
	else
		val &= ~SPI_IO_CONTROL_CLOCK_IDLE_HIGH;

	write32(ds->regs->io_control, val);
}
/*
 * Reset entire QUP and all mini cores
 */
static void spi_reset(struct ipq_spi_slave *ds)
{
	write32(ds->regs->qup_sw_reset, 0x1);
	udelay(5);
}

static struct ipq_spi_slave spi_slave_pool[2];

void spi_init(void)
{
	/* just in case */
	memset(spi_slave_pool, 0, sizeof(spi_slave_pool));

}

void spi_setup_slave(unsigned int bus, unsigned int cs,
			struct ipq_spi_slave *ds)
{
	/*
	 * QCA BLSP supports SPI Flash
	 * on different BLSP0 and BLSP1
	 * with different number of chip selects (CS, channels):
	 */
	if ((bus < BLSP0_SPI) || (bus > BLSP1_SPI)
		|| ((bus == BLSP0_SPI) && (cs > 2))
		|| ((bus == BLSP1_SPI) && (cs > 0))) {
		printf(
			"SPI error: unsupported bus %d (Supported busses 0, 1 and 2) or chipselect\n",
			bus);
		return;
	}

	ds->slave.bus	= bus;
	ds->slave.cs	= cs;
	ds->regs	= &spi_reg[bus];

	/*
	 * TODO(vbendeb):
	 * hardcoded frequency and mode - we might need to find a way
	 * to configure this
	 */
	ds->freq = 10000000;
	ds->mode = SPI_MODE3;
}

/*
 * BLSP QUPn SPI Hardware Initialisation
 */
static int spi_hw_init(struct ipq_spi_slave *ds)
{
	int ret;

	ds->initialized = 0;

	/* QUPn module configuration */
	spi_reset(ds);

	/* Set the QUPn state */
	ret = config_spi_state(ds, SPI_RESET_STATE);
	if (ret)
		return ret;

	/*
	 * Configure Mini core to SPI core with Input Output enabled,
	 * SPI master, N = 8 bits
	 */
	clrsetbits_le32(ds->regs->qup_config, (QUP_CONFIG_MINI_CORE_MSK |
					       QUP_CONF_INPUT_MSK |
					       QUP_CONF_OUTPUT_MSK |
					       SPI_BIT_WORD_MSK),
					      (QUP_CONFIG_MINI_CORE_SPI |
					       QUP_CONF_INPUT_ENA |
					       QUP_CONF_OUTPUT_ENA |
					       SPI_8_BIT_WORD));

	/*
	 * Configure Input first SPI protocol,
	 * SPI master mode and no loopback
	 */
	clrsetbits_le32(ds->regs->spi_config, (LOOP_BACK_MSK |
					       SLAVE_OPERATION_MSK),
					      (NO_LOOP_BACK |
					       SLAVE_OPERATION));

	/*
	 * Configure SPI IO Control Register
	 * CLK_ALWAYS_ON = 0
	 * MX_CS_MODE = 0
	 * NO_TRI_STATE = 1
	 */
	write32(ds->regs->io_control, (CLK_ALWAYS_ON | NO_TRI_STATE));

	/*
	 * Configure SPI IO Modes.
	 * OUTPUT_BIT_SHIFT_EN = 1
	 * INPUT_MODE = Block Mode
	 * OUTPUT MODE = Block Mode
	 */
	clrsetbits_le32(ds->regs->qup_io_modes, (OUTPUT_BIT_SHIFT_MSK |
						 INPUT_BLOCK_MODE_MSK |
						 OUTPUT_BLOCK_MODE_MSK),
						(OUTPUT_BIT_SHIFT_EN |
						 INPUT_BLOCK_MODE |
						 OUTPUT_BLOCK_MODE));

	spi_set_mode(ds, ds->mode);

	/* Disable Error mask */
	write32(ds->regs->error_flags_en, 0);
	write32(ds->regs->qup_error_flags_en, 0);

	write32(ds->regs->qup_deassert_wait, 0);

	ds->initialized = 1;

	return SUCCESS;
}

static int spi_claim_bus(SpiOps *spi_ops)
{
	struct ipq_spi_slave *ds = to_ipq_spi(spi_ops);
	unsigned int ret;

	ret = spi_hw_init(ds);
	if (ret)
		return -EIO;

	write_force_cs(spi_ops, 1);

	ret = config_spi_state(ds, SPI_RESET_STATE);
	if (ret != SUCCESS)
		return ret;

	return SUCCESS;
}

static int spi_release_bus(SpiOps *ops)
{
	struct ipq_spi_slave *ds = to_ipq_spi(ops);

	write_force_cs(ops, 0);

	/* Reset the SPI hardware */
	spi_reset(ds);
	ds->initialized = 0;

	return 0;
}

static void write_force_cs(SpiOps *ops, int assert)
{
	struct ipq_spi_slave *ds = to_ipq_spi(ops);

	if (assert)
		clrsetbits_le32(ds->regs->io_control,
			FORCE_CS_MSK, FORCE_CS_EN);
	else
		clrsetbits_le32(ds->regs->io_control,
			FORCE_CS_MSK, FORCE_CS_DIS);

	return;

}

/*
 * Function to write data to OUTPUT FIFO
 */
static void spi_write_byte(struct ipq_spi_slave *ds, unsigned char data)
{
	/* Wait for space in the FIFO */
	while ((read32(ds->regs->qup_operational) & QUP_OUTPUT_FIFO_FULL))
		udelay(1);

	/* Write the byte of data */
	write32(ds->regs->qup_output_fifo, data);
}

/*
 * Function to read data from Input FIFO
 */
static unsigned char spi_read_byte(struct ipq_spi_slave *ds)
{
	/* Wait for Data in FIFO */
	while (!(read32(ds->regs->qup_operational) &
			QUP_DATA_AVAILABLE_FOR_READ)) {
		udelay(1);
	}

	/* Read a byte of data */
	return read32(ds->regs->qup_input_fifo) & 0xff;
}

/*
 * Function to check wheather Input or Output FIFO
 * has data to be serviced
 */
static int check_fifo_status(void *reg_addr)
{
	unsigned int count = TIMEOUT_CNT;
	unsigned int status_flag;
	unsigned int val;

	do {
		val = read32(reg_addr);
		count--;
		if (count == 0)
			return -ETIMEDOUT;
		status_flag = ((val & OUTPUT_SERVICE_FLAG) |
				(val & INPUT_SERVICE_FLAG));
	} while (!status_flag);

	return SUCCESS;
}

/*
 * Function to configure Input and Output enable/disable
 */
static void enable_io_config(struct ipq_spi_slave *ds,
				uint32_t write_cnt, uint32_t read_cnt)
{

	if (write_cnt) {
		clrsetbits_le32(ds->regs->qup_config,
				QUP_CONF_OUTPUT_MSK, QUP_CONF_OUTPUT_ENA);
	} else {
		clrsetbits_le32(ds->regs->qup_config,
				QUP_CONF_OUTPUT_MSK, QUP_CONF_NO_OUTPUT);
	}

	if (read_cnt) {
		clrsetbits_le32(ds->regs->qup_config,
				QUP_CONF_INPUT_MSK, QUP_CONF_INPUT_ENA);
	} else {
		clrsetbits_le32(ds->regs->qup_config,
				QUP_CONF_INPUT_MSK, QUP_CONF_NO_INPUT);
	}
}

/*
 * Function to read bytes number of data from the Input FIFO
 */
static int __blsp_spi_read(struct ipq_spi_slave *ds, u8 *data_buffer,
				unsigned int bytes)
{
	uint32_t val;
	unsigned int i;
	unsigned int read_bytes = bytes;
	unsigned int fifo_count;
	int ret = SUCCESS;
	int state_config;

	/* Configure no of bytes to read */
	state_config = config_spi_state(ds, SPI_RESET_STATE);
	if (state_config)
		return state_config;

	/* Configure input and output enable */
	enable_io_config(ds, 0, read_bytes);

	write32(ds->regs->qup_mx_input_count, bytes);

	state_config = config_spi_state(ds, SPI_RUN_STATE);
	if (state_config)
		return state_config;

	while (read_bytes) {
		ret = check_fifo_status(ds->regs->qup_operational);
		if (ret != SUCCESS)
			goto out;

		val = read32(ds->regs->qup_operational);
		if (val & INPUT_SERVICE_FLAG) {
			/*
			 * acknowledge to hw that software will
			 * read input data
			 */
			val &= INPUT_SERVICE_FLAG;
			write32(ds->regs->qup_operational, val);

			fifo_count = ((read_bytes > SPI_INPUT_BLOCK_SIZE) ?
					SPI_INPUT_BLOCK_SIZE : read_bytes);

			for (i = 0; i < fifo_count; i++) {
				*data_buffer = spi_read_byte(ds);
				data_buffer++;
				read_bytes--;
			}
		}
	}

 out:
	/*
	 * Put the SPI Core back in the Reset State
	 * to end the transfer
	 */
	(void)config_spi_state(ds, SPI_RESET_STATE);

	return ret;

}

static int blsp_spi_read(struct ipq_spi_slave *ds, u8 *data_buffer,
				unsigned int bytes)
{
	int length, ret;

	while (bytes) {
		length = (bytes < MAX_COUNT_SIZE) ? bytes : MAX_COUNT_SIZE;

		ret = __blsp_spi_read(ds, data_buffer, length);
		if (ret != SUCCESS)
			return ret;

		data_buffer += length;
		bytes -= length;
	}

	return 0;
}

/*
 * Function to write data to the Output FIFO
 */
static int __blsp_spi_write(struct ipq_spi_slave *ds, const u8 *cmd_buffer,
				unsigned int bytes)
{
	uint32_t val;
	unsigned int i;
	unsigned int write_len = bytes;
	unsigned int read_len = bytes;
	unsigned int fifo_count;
	int ret = SUCCESS;
	int state_config;

	state_config = config_spi_state(ds, SPI_RESET_STATE);
	if (state_config)
		return state_config;

	/* No of bytes to be written in Output FIFO */
	write32(ds->regs->qup_mx_output_count, bytes);
	write32(ds->regs->qup_mx_input_count, bytes);
	state_config = config_spi_state(ds, SPI_RUN_STATE);
	if (state_config)
		return state_config;

	/* Configure input and output enable */
	enable_io_config(ds, write_len, read_len);

	/*
	 * read_len considered to ensure that we read the dummy data for the
	 * write we performed. This is needed to ensure with WR-RD transaction
	 * to get the actual data on the subsequent read cycle that happens
	 */
	while (write_len || read_len) {

		ret = check_fifo_status(ds->regs->qup_operational);
		if (ret != SUCCESS)
			goto out;

		val = read32(ds->regs->qup_operational);
		if (val & OUTPUT_SERVICE_FLAG) {
			/*
			 * acknowledge to hw that software will write
			 * expected output data
			 */
			val &= OUTPUT_SERVICE_FLAG;
			write32(ds->regs->qup_operational, val);

			if (write_len > SPI_OUTPUT_BLOCK_SIZE)
				fifo_count = SPI_OUTPUT_BLOCK_SIZE;
			else
				fifo_count = write_len;

			for (i = 0; i < fifo_count; i++) {
				/* Write actual data to output FIFO */
				spi_write_byte(ds, *cmd_buffer);
				cmd_buffer++;
				write_len--;
			}
		}
		if (val & INPUT_SERVICE_FLAG) {
			/*
			 * acknowledge to hw that software
			 * will read input data
			 */
			val &= INPUT_SERVICE_FLAG;
			write32(ds->regs->qup_operational, val);

			if (read_len > SPI_INPUT_BLOCK_SIZE)
				fifo_count = SPI_INPUT_BLOCK_SIZE;
			else
				fifo_count = read_len;

			for (i = 0; i < fifo_count; i++) {
				/* Read dummy data for the data written */
				(void)spi_read_byte(ds);

				/*
				 * Decrement the write count after reading the
				 * dummy data from the device. This is to make
				 * sure we read dummy data before we write the
				 * data to fifo
				 */
				read_len--;
			}
		}
	}

 out:
	/*
	 * Put the SPI Core back in the Reset State
	 * to end the transfer
	 */
	(void)config_spi_state(ds, SPI_RESET_STATE);

	return ret;
}

static int blsp_spi_write(struct ipq_spi_slave *ds, u8 *cmd_buffer,
			  unsigned int bytes)
{
	int length, ret;

	while (bytes) {
		length = (bytes < MAX_COUNT_SIZE) ? bytes : MAX_COUNT_SIZE;

		ret = __blsp_spi_write(ds, cmd_buffer, length);
		if (ret != SUCCESS)
			return ret;

		cmd_buffer += length;
		bytes -= length;
	}

	return 0;
}

unsigned int spi_crop_chunk(unsigned int cmd_len, unsigned int buf_len)
{
	return (MAX_PACKET_COUNT < buf_len)? MAX_PACKET_COUNT : buf_len;
}
/*
 * This function is invoked with either tx_buf or rx_buf.
 * Calling this function with both null does a chip select change.
 */
int spi_xfer(SpiOps *ops, void *din, const void *dout, uint32_t bytes)
{
	struct ipq_spi_slave *ds = to_ipq_spi(ops);
	u8 *txp = (u8 *)dout;
	u8 *rxp = (u8 *)din;
	int ret;

	if (dout != NULL) {
		ret = blsp_spi_write(ds, txp, bytes);
		if (ret != SUCCESS)
			return ret;
	}

	if (din != NULL) {
		ret = blsp_spi_read(ds, rxp, bytes);
		if (ret != SUCCESS)
			return ret;
	}

	return ret;
}

SpiController *new_spi(unsigned bus_num, unsigned cs)
{
	SpiController *bus = xzalloc(sizeof(*bus));

	if (bus) {
		bus->ops.start = spi_claim_bus;
		bus->ops.transfer = spi_xfer;
		bus->ops.stop = spi_release_bus;
		spi_setup_slave(bus_num, cs, &bus->ipq_spi_slave);
	}

	return bus;
}
