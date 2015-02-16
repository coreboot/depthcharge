/*
 * Copyright 2012-2014 Imagination Technologies Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include <assert.h>
#include <config.h>
#include <libpayload.h>
#include "base/container_of.h"
#include "drivers/bus/i2c/imgtec_i2c.h"
#include "drivers/bus/i2c/i2c.h"

#define SCB_LINE_STATUS_REG_OFFSET	0x00
#define SCB_READ_ADDR_REG_OFFSET	0x08
#define SCB_READ_COUNT_REG_OFFSET	0x0c
#define SCB_WRITE_ADDR_REG_OFFSET	0x10
#define SCB_READ_DATA_REG_OFFSET	0x14
#define SCB_WRITE_DATA_REG_OFFSET	0x18
#define SCB_FIFO_STATUS_REG_OFFSET	0x1c
#define SCB_CLK_SET_REG_OFFSET		0x3c
#define SCB_INT_STATUS_REG_OFFSET	0x40
#define SCB_INT_CLEAR_REG_OFFSET	0x44
#define SCB_INT_MASK_REG_OFFSET		0x48
#define SCB_CONTROL_REG_OFFSET		0x4c
#define SCB_TIME_TPL_REG_OFFSET		0x50
#define SCB_TIME_TPH_REG_OFFSET		0x54
#define SCB_TIME_TP2S_REG_OFFSET	0x58
#define SCB_TIME_TBI_REG_OFFSET		0x60
#define SCB_TIME_TSL_REG_OFFSET		0x64
#define SCB_TIME_TDL_REG_OFFSET		0x68
#define SCB_TIME_TSDL_REG_OFFSET	0x6c
#define SCB_TIME_TSDH_REG_OFFSET	0x70
#define SCB_WRITE_COUNT_REG_OFFSET	0x7c
#define SCB_CORE_REV_REG_OFFSET		0x80
#define SCB_TIME_TCKH_REG_OFFSET	0x84
#define SCB_TIME_TCKL_REG_OFFSET	0x88
#define SCB_READ_FIFO_REG_OFFSET	0x94
#define SCB_CLEAR_STATUS_OFFSET		0x98

/* SCB_CONTROL_REG bits */
#define SCB_CONTROL_CLK_ENABLE		0x1e0
#define SCB_CONTROL_NORMAL_OP		0x01f
#define SCB_CONTROL_TRANSACTION_HALT	0x200

/* SCB_FIFO_STATUS bits */
#define STATUS_FIFO_READ_EMPTY		0x2
#define STATUS_FIFO_WRITE_FULL		0x4

/* SCB_CLK_SET_REG bits */
#define SCB_FILT_DISABLE		0x80000000
#define SCB_FILT_BYPASS			0x40000000
#define SCB_FILT_INC_MASK		0x0000007f
#define SCB_FILT_INC_SHIFT		16
#define SCB_INC_MASK			0x0000007f
#define SCB_INC_SHIFT			8

/* SCB_INT_*_REG bits */
#define INT_BUS_INACTIVE		0x00001
#define INT_UNEXPECTED_START		0x00002
#define INT_SCLK_LOW_TIMEOUT		0x00004
#define INT_SDAT_LOW_TIMEOUT		0x00008
#define INT_WRITE_ACK_ERR		0x00010
#define INT_ADDR_ACK_ERR		0x00020
#define INT_FIFO_FULL			0x00200
#define INT_FIFO_FILLING		0x00400
#define INT_FIFO_EMPTY			0x00800
#define INT_FIFO_EMPTYING		0x01000
#define INT_SLAVE_EVENT			0x10000
#define INT_THALT			0x20000
#define INT_TDONE			0x80000

/* SCB_LINE_STATUS bits */
#define LINESTAT_START_BIT_DET		0x00002000
#define LINESTAT_STOP_BIT_DET		0x00004000

#define INT_ENABLE_MASK (INT_UNEXPECTED_START	| \
			INT_SCLK_LOW_TIMEOUT	| \
			INT_SDAT_LOW_TIMEOUT	| \
			INT_WRITE_ACK_ERR	| \
			INT_ADDR_ACK_ERR	| \
			INT_FIFO_FULL		| \
			INT_FIFO_FILLING	| \
			INT_FIFO_EMPTY		| \
			INT_FIFO_EMPTYING	| \
			INT_THALT		| \
			INT_TDONE)

#define LINESTAT_INPUT_DATA		0xff000000
#define LINESTAT_CLEAR_SHIFT		13
#define LINESTAT_LATCHED		(0x3f << LINESTAT_CLEAR_SHIFT)

/*
 * Worst incs are 1 (innacurate) and 16*256 (irregular).
 * So a sensible inc is the logarithmic mean: 64 (2^6), which is
 * in the middle of the valid range (0-127).
 */
#define SCB_OPT_INC			64

/* Setup the clock enable filtering for 25 ns */
#define SCB_FILT_GLITCH			25

enum {
	I2C_OK = 0,
	I2C_UNEXPECTED_START = 1,
	I2C_SCLK_LOW_TIMEOUT = 2,
	I2C_SDAT_LOW_TIMEOUT = 3,
	I2C_WRITE_ACK_ERR  = 4,
	I2C_ADDR_ACK_ERR = 5,
	I2C_BUS_INACTIVE = 6,
	I2C_IO_ERROR = 7,
	I2C_TIMEOUT_ERROR = 8
};

#define IMG_I2C_BASE_ADDRESS(i)		(0xB8100000 + (0x200*(i)))

/* Local status bits */
#define LOCAL_STATUS_STARTED		0x01
#define LOCAL_STATUS_LAST_SEG		0x02
#define LOCAL_STATUS_WAIT_STOP_BIT	0x04
#define LOCAL_STATUS_ALL_FINISHED	0x08
#define LOCAL_STATUS_THALT		0x10
#define LOCAL_EXIT_STATUS		((LOCAL_STATUS_WAIT_STOP_BIT) | \
						(LOCAL_STATUS_ALL_FINISHED))

#define I2C_TIMEOUT_VALUE_US		500000

/* Timing parameters for i2c modes (in ns) */
struct i2c_timings {
	const char *name;
	uint32_t max_bitrate;
	uint32_t tckh, tckl, tsdh, tsdl;
	uint32_t tp2s, tpl, tph;
	uint32_t tbi, tsl, tdl;
};

static struct i2c_timings timings[] = {
	/* Standard mode */
	{
		.name = "standard",
		.max_bitrate = 100000,
		.tckh = 4000,
		.tckl = 4700,
		.tsdh = 4700,
		.tsdl = 8700,
		.tp2s = 4700,
		.tpl = 4700,
		.tph = 4000,
		/* These timeouts are an issue only when they go on
		 * indefinetely; disable them and just make sure the
		 * isr function does not get stuck in a loop */
		.tbi = 0x0,
		.tsl = 0x0,
		.tdl = 0x0
	},
};

/* i2c_set_bus_speed - Set the i2c speed. */
static void imgtec_i2c_set_bus_speed(I2cOps *me)
{

	unsigned clk_khz, bitrate_khz, clk_period, tckh, tckl, tsdh;
	unsigned i, data, prescale, inc, int_bitrate, filt;
	struct i2c_timings timing;
	ImgI2c *bus = container_of(me, ImgI2c, ops);
	uint32_t base = bus->base;

	bitrate_khz = bus->bitrate / 1000;
	clk_khz = bus->clkrate / 1000;

	/* Determine what mode we're in from the bitrate */
	timing = timings[0];
	for (i = 0; i < ARRAY_SIZE(timings); i++) {
		if (bus->bitrate <= timings[i].max_bitrate) {
			timing = timings[i];
			break;
		}
	}

	/* Find the prescale that would give us that inc (approx delay = 0) */
	prescale = (SCB_OPT_INC * clk_khz) / (256 * 16 * bitrate_khz);
	prescale = clamp_t(unsigned, prescale, 1, 8);
	clk_khz /= prescale;

	/* Setup the clock increment value */
	inc = (256 * 16 * bitrate_khz) / clk_khz;

	/*
	 * The clock generation logic allows to filter glitches on the bus.
	 * This filter is able to remove bus glitches shorter than 50ns.
	 * If the clock enable rate is greater than 20 MHz, no filtering
	 * is required, so we need to disable it.
	 * If it's between the 20-40 MHz range, there's no need to divide
	 * the clock to get a filter.
	 */
	if (clk_khz < 20000) {
		filt = SCB_FILT_DISABLE;
	} else if (clk_khz < 40000) {
		filt = SCB_FILT_BYPASS;
	} else {
		/* Calculate filter clock */
		filt = (64000 / ((clk_khz / 1000) * SCB_FILT_GLITCH));

		/* Scale up if needed */
		if (64000 % ((clk_khz / 1000) * SCB_FILT_GLITCH))
			inc++;
		if (inc > SCB_INC_MASK)
			inc = SCB_INC_MASK;
		if (filt > SCB_FILT_INC_MASK)
			filt = SCB_FILT_INC_MASK;

		filt = (filt & SCB_FILT_INC_MASK) << SCB_FILT_INC_SHIFT;
	}

	data = filt | ((inc & SCB_INC_MASK) << SCB_INC_SHIFT) | (prescale - 1);
	write32(data, base + SCB_CLK_SET_REG_OFFSET);

	/* Obtain the clock period of the fx16 clock in ns */
	clk_period = (256 * 1000000) / (clk_khz * inc);

	/* Calculate the bitrate in terms of internal clock pulses */
	int_bitrate = 1000000 / (bitrate_khz * clk_period);
	if ((1000000 % (bitrate_khz * clk_period)) >=
	    ((bitrate_khz * clk_period) / 2))
		int_bitrate++;

	/* Setup TCKH value */
	tckh = timing.tckh / clk_period;
	if (timing.tckh % clk_period)
		tckh++;
	data = (tckh > 0) ? (tckh - 1) : 0;
	write32(data, base + SCB_TIME_TCKH_REG_OFFSET);

	/* Setup TCKL value */
	tckl = int_bitrate - tckh;
	data = (tckl > 0) ? (tckl - 1) : 0;
	write32(data, base + SCB_TIME_TCKL_REG_OFFSET);

	/* Setup TSDH value */
	tsdh = timing.tsdh / clk_period;
	if (timing.tsdh % clk_period)
		tsdh++;
	data = (tsdh > 1) ? (tsdh - 1) : 1;
	write32(data, base + SCB_TIME_TSDH_REG_OFFSET);

	/* This value is used later */
	tsdh = data;

	/* Setup TPL value */
	data = timing.tpl / clk_period;
	if (data > 0)
		--data;
	write32(data, base + SCB_TIME_TPL_REG_OFFSET);

	/* Setup TPH value */
	data = timing.tph / clk_period;
	if (data > 0)
		--data;
	write32(data, base + SCB_TIME_TPH_REG_OFFSET);

	/* Setup TSDL value to TPL + TSDH + 2 */
	write32(data + tsdh + 2, base + SCB_TIME_TSDL_REG_OFFSET);

	/* Setup TP2S value */
	data = timing.tp2s / clk_period;
	if (data > 0)
		--data;
	write32(data, base + SCB_TIME_TP2S_REG_OFFSET);

	write32(timing.tbi, base + SCB_TIME_TBI_REG_OFFSET);
	write32(timing.tsl, base + SCB_TIME_TSL_REG_OFFSET);
	write32(timing.tdl, base + SCB_TIME_TDL_REG_OFFSET);
}

/*
 * The code to read from the master read fifo, and write to the master
 * write fifo, checks a bit in an SCB register before every byte to
 * ensure that the fifo is not full (write fifo) or empty (read fifo).
 * Due to clock domain crossing inside the SCB block the updated value
 * of this bit is only visible after 2 cycles.
 *
 * The i2c_wr_rd_fence() function does 2 dummy writes (to the read-only
 * revision register), and it's called after reading from or writing to the
 * fifos to ensure that subsequent reads of the fifo status bits do not read
 * stale values.
 */
static void i2c_wr_rd_fence(uint32_t base)
{
	write32(0, base + SCB_CORE_REV_REG_OFFSET);
	write32(0, base + SCB_CORE_REV_REG_OFFSET);
}

/* Enable or release transaction halt for control of repeated starts */
static void img_i2c_transaction_halt(I2cOps *me, uint8_t t_halt)
{
	ImgI2c *bus = container_of(me, ImgI2c, ops);
	uint32_t reg;
	uint8_t required_status;

	/* Nothing to do if current status matches required status. */
	required_status = t_halt ? LOCAL_STATUS_THALT : 0;
	if ((bus->local_status & LOCAL_STATUS_THALT) == required_status)
		return;

	reg = read32(bus->base + SCB_CONTROL_REG_OFFSET);
	if (t_halt) {
		reg |= SCB_CONTROL_TRANSACTION_HALT;
		bus->local_status |= LOCAL_STATUS_THALT;
	} else {
		reg &= ~SCB_CONTROL_TRANSACTION_HALT;
		bus->local_status &= ~LOCAL_STATUS_THALT;
	}
	write32(reg, bus->base + SCB_CONTROL_REG_OFFSET);
}

/* Perform a read transaction. */
static void imgtec_i2c_read(I2cOps *me, I2cSeg *seg)
{
	ImgI2c *bus = container_of(me, ImgI2c, ops);
	if (!(bus->local_status & LOCAL_STATUS_LAST_SEG)) {
		bus->int_enable |= INT_SLAVE_EVENT;
		/* Update enabled interrupts */
		write32(bus->int_enable, bus->base + SCB_INT_MASK_REG_OFFSET);
	}
	/* Check if this is the start of a new read to this address */
	if (!(bus->local_status & LOCAL_STATUS_STARTED)) {
		/* Specify address */
		write32(seg->chip, bus->base + SCB_READ_ADDR_REG_OFFSET);
		/* Specify read count */
		write32(seg->len, bus->base + SCB_READ_COUNT_REG_OFFSET);
		/* Mark transaction as started */
		bus->local_status |= LOCAL_STATUS_STARTED;
		/* Initialize temporary holders */
		bus->current_len = seg->len;
		bus->current_buf = seg->buf;
		return;
	}
	while (bus->current_len) {
		/* If the FIFO is empty exit the loop */
		if (read32(bus->base + SCB_FIFO_STATUS_REG_OFFSET) &
						STATUS_FIFO_READ_EMPTY)
			break;
		*(bus->current_buf) = read32(bus->base +
					SCB_READ_DATA_REG_OFFSET);
		/* Advance FIFO */
		write32(0xFF, bus->base + SCB_READ_FIFO_REG_OFFSET);
		i2c_wr_rd_fence(bus->base);
		(bus->current_len)--;
		(bus->current_buf)++;
	}

}

/* Perform a write transaction. */
static void imgtec_i2c_write(I2cOps *me, I2cSeg *seg)
{
	ImgI2c *bus = container_of(me, ImgI2c, ops);

	if (!(bus->local_status & LOCAL_STATUS_LAST_SEG))
		bus->int_enable |= INT_SLAVE_EVENT;

	/* Check if this is the start of a new write to this address */
	if (!(bus->local_status & LOCAL_STATUS_STARTED)) {
		/* Specify address */
		write32(seg->chip, bus->base + SCB_WRITE_ADDR_REG_OFFSET);
		/* Specify write count */
		write32(seg->len, bus->base + SCB_WRITE_COUNT_REG_OFFSET);
		/* Mark transaction as started */
		bus->local_status |= LOCAL_STATUS_STARTED;
		/* Initialize temporary holders */
		bus->current_len = seg->len;
		bus->current_buf = seg->buf;
	}
	while (bus->current_len) {
		if (read32(bus->base + SCB_FIFO_STATUS_REG_OFFSET) &
						STATUS_FIFO_WRITE_FULL)
			break;
		write32(*(bus->current_buf), bus->base +
					SCB_WRITE_DATA_REG_OFFSET);
		i2c_wr_rd_fence(bus->base);
		(bus->current_len)--;
		(bus->current_buf)++;
	}

	if (!bus->current_len)
		/* Disable fifo emptying interrupt if nothing more to write */
		bus->int_enable &= ~INT_FIFO_EMPTYING;

	write32(bus->int_enable, bus->base + SCB_INT_MASK_REG_OFFSET);
}

static int imgtec_i2c_isr(I2cOps *me, I2cSeg *seg)
{
	uint32_t int_status, line_status;
	ImgI2c *bus = container_of(me, ImgI2c, ops);
	uint32_t base = bus->base;

	/* Read interrupt status register. */
	int_status = read32(base + SCB_INT_STATUS_REG_OFFSET);
	if (!int_status)
		return I2C_OK;
	/* Disable all interrupts */
	write32(0, base + SCB_INT_MASK_REG_OFFSET);

	/* Clear detected interrupts. */
	write32(int_status, base + SCB_INT_CLEAR_REG_OFFSET);

	/*
	 * Read line status and clear it until it actually is clear.  We have
	 * to be careful not to lose any line status bits that get latched.
	 */
	line_status = read32(base + SCB_LINE_STATUS_REG_OFFSET);
	if (line_status & LINESTAT_LATCHED) {
		write32((line_status & LINESTAT_LATCHED) >>
			LINESTAT_CLEAR_SHIFT, base + SCB_CLEAR_STATUS_OFFSET);
		i2c_wr_rd_fence(base);
	}

	/* Unexpected start bit interrupt */
	if (int_status & INT_UNEXPECTED_START) {
		printf("%s: Error: Unexpected start bit interrupt.\n",
				__func__);
		return -I2C_UNEXPECTED_START;
	}
	/* Sclk timeout exceeded
	 * Certain interrupts indicate that sclk low timeout is not
	 * a problem. If any of these are set, just continue.
	 */
	if ((int_status & INT_SCLK_LOW_TIMEOUT) && !(int_status &
	(INT_SLAVE_EVENT | INT_FIFO_EMPTY | INT_FIFO_FULL))) {
		printf("%s: Error: Sclk timeout exceeded.\n", __func__);
		return -I2C_SCLK_LOW_TIMEOUT;
	}
	/* Sdat low timeout exceeded */
	if (int_status & INT_SDAT_LOW_TIMEOUT) {
		printf("%s: Error:  Sdat low timeout exceeded.\n", __func__);
		return -I2C_SDAT_LOW_TIMEOUT;
	}
	/* Write acknowledge error interrupt */
	if (int_status & INT_WRITE_ACK_ERR) {
		printf("%s: Error: Write acknowledge error.\n", __func__);
		return -I2C_WRITE_ACK_ERR;
	}
	/* Address acknowledge error interrupt */
	if (int_status & INT_ADDR_ACK_ERR) {
		/* Suppress error message while scanning. */
		if (!bus->scan_mode)
			printf("%s: Error: Address acknowledge error.\n",
			       __func__);
		return -I2C_ADDR_ACK_ERR;
	}
	if ((int_status & (INT_BUS_INACTIVE)) && (bus->current_len > 0)) {
		printf("%s: Error: Bus inactive.\n", __func__);
		return -I2C_BUS_INACTIVE;
	}

	/* If I have all data in the FIFO and THALT was generated,
	 * I consider current transaction finished
	 */
	if ((int_status & INT_THALT) && (bus->current_len == 0))
		bus->local_status |= LOCAL_STATUS_ALL_FINISHED;

	/* We only re-enable halt if we have other transactions
	 * after this one */
	if (!(bus->local_status & LOCAL_STATUS_LAST_SEG) &&
		(line_status & LINESTAT_START_BIT_DET)) {
		/* For the first segment this is already set so nothing
		 * changes */
		img_i2c_transaction_halt(me, 1);
		/* We're no longer interested in the slave event */
		bus->int_enable &= ~INT_SLAVE_EVENT;
		int_status &= ~INT_SLAVE_EVENT;
	}
	/*
	 * If transfer is finished (stop detected) or the fifo is full or
	 * filling read the remaining data
	 */
	if (seg->read && !(bus->local_status & LOCAL_EXIT_STATUS) &&
		(int_status & (INT_TDONE | INT_FIFO_FULL | INT_FIFO_FILLING))) {
		imgtec_i2c_read(me, seg);
		if (bus->current_len == 0) {
			if (int_status & INT_TDONE)
				bus->local_status |= LOCAL_STATUS_ALL_FINISHED;
			else
				bus->local_status |= LOCAL_STATUS_WAIT_STOP_BIT;
		}
	}
	/*
	 * The write fifo empty indicates that we're in the
	 * last byte so it's safe to start a new write
	 * transaction without losing any bytes from the
	 * previous one.
	 * see 2.3.7 Repeated Start Transactions.
	 */
	if (!seg->read && !(bus->local_status & LOCAL_EXIT_STATUS) &&
		(int_status & (INT_FIFO_EMPTY | INT_FIFO_EMPTYING))) {
		if ((int_status & INT_FIFO_EMPTY) && (bus->current_len == 0))
			bus->local_status |= LOCAL_STATUS_WAIT_STOP_BIT;
		else
			imgtec_i2c_write(me, seg);
	}

	/* Only wait for stop on last message.
	 * Also we may already have detected the stop bit.
	 */
	if ((bus->local_status & LOCAL_STATUS_WAIT_STOP_BIT) &&
		(line_status & LINESTAT_STOP_BIT_DET)) {
		if (bus->local_status & LOCAL_STATUS_LAST_SEG)
			bus->local_status |= LOCAL_STATUS_ALL_FINISHED;
		else if (int_status & INT_SLAVE_EVENT)
			bus->local_status |= LOCAL_STATUS_ALL_FINISHED;
	}

	/* Enable interrupts (int_enable may be altered by changing mode) */
	write32(bus->int_enable, base + SCB_INT_MASK_REG_OFFSET);

	return I2C_OK;
}

/* imgtec_i2c_init - Init function. */
static void imgtec_i2c_init(I2cOps *me)
{
	ImgI2c *bus = container_of(me, ImgI2c, ops);
	uint32_t base = bus->base;

	imgtec_i2c_set_bus_speed(me);

	/* Take module out of soft reset and enable clocks */
	write32(0, base + SCB_CONTROL_REG_OFFSET);
	write32(SCB_CONTROL_CLK_ENABLE | SCB_CONTROL_NORMAL_OP,
		base + SCB_CONTROL_REG_OFFSET);

	/* Disable all interrupts */
	write32(0, base + SCB_INT_MASK_REG_OFFSET);

	/* Clear all interrupts */
	write32(~0, base + SCB_INT_CLEAR_REG_OFFSET);

	/* Clear the scb_line_status events */
	write32(~0, base + SCB_CLEAR_STATUS_OFFSET);

	/* Enable interrupts */
	bus->int_enable = INT_ENABLE_MASK;
	write32(bus->int_enable, base + SCB_INT_MASK_REG_OFFSET);

	bus->initialized = 1;
}

/* imgtec_i2c_transfer - Read / Write from I2c registers. */
static int imgtec_i2c_transfer(I2cOps *me, I2cSeg *segments, int seg_count)
{
	ImgI2c *bus = container_of(me, ImgI2c, ops);
	I2cSeg *seg = segments;
	int ret = I2C_OK, i;
	uint64_t start_timer;

	if (!bus->initialized)
		imgtec_i2c_init(me);

	bus->local_status = 0;
	if (seg_count > 1)
		img_i2c_transaction_halt(me, 1);
	for (i = 0; (i < seg_count) && (!ret); i++, seg++) {
		/*
		 * After the last message we must have waited for a stop bit.
		 * Not waiting can cause problems when the clock is disabled
		 * before the stop bit is sent, and the I2C interface requires
		 * separate transfers not to be joined with repeated start.
		 */
		bus->local_status &= LOCAL_STATUS_THALT;
		bus->local_status |= (i == seg_count - 1) ?
					LOCAL_STATUS_LAST_SEG : 0;
		if (seg->read)
			imgtec_i2c_read(me, seg);
		else
			imgtec_i2c_write(me, seg);
		if (i > 0) {
			/* If we have more than one segment we release the
			 * halt to carry with current transaction */
			img_i2c_transaction_halt(me, 0);
		}
		start_timer = timer_us(0);
		while (!(bus->local_status & LOCAL_STATUS_ALL_FINISHED) &&
								(!ret)) {
			ret = imgtec_i2c_isr(me, seg);
			if (timer_us(start_timer) > I2C_TIMEOUT_VALUE_US)
				ret = -I2C_TIMEOUT_ERROR;
		}
		/* Disable all interrupts */
		write32(0, bus->base + SCB_INT_MASK_REG_OFFSET);
	}
	/* Clear halt if left enabled */
	img_i2c_transaction_halt(me, 0);
	return ret;
}

static void imgtec_scan_mode_on_off(struct I2cOps *me, int mode_on)
{
	ImgI2c *bus = container_of(me, ImgI2c, ops);
	bus->scan_mode = mode_on;
}

/* new_imgtec_i2c - Allocate new i2c bus. */
ImgI2c *new_imgtec_i2c(unsigned port, unsigned bitrate, unsigned clkrate)
{
	ImgI2c *bus = xzalloc(sizeof(*bus));

	bus->ops.transfer = &imgtec_i2c_transfer;
	bus->ops.scan_mode_on_off = imgtec_scan_mode_on_off;
	bus->base = IMG_I2C_BASE_ADDRESS(port);
	bus->bitrate = bitrate;
	bus->clkrate = clkrate;

	if (CONFIG_CLI)
		add_i2c_controller_to_list(&bus->ops, "Imgtec%d", port);

	return bus;
}
