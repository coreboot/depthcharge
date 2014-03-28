/*
 * (C) Copyright 2009
 * Vipin Kumar, ST Micoelectronics, vipin.kumar@st.com.
 * Copyright 2013 Google Inc.
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
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <libpayload.h>

#include "base/container_of.h"
#include "drivers/bus/i2c/designware.h"

typedef struct {
	uint32_t control;
	uint32_t target_addr;
	uint32_t slave_addr;
	uint32_t master_addr;
	uint32_t cmd_data;
	uint32_t ss_scl_hcnt;
	uint32_t ss_scl_lcnt;
	uint32_t fs_scl_hcnt;
	uint32_t fs_scl_lcnt;
	uint32_t hs_scl_hcnt;
	uint32_t hs_scl_lcnt;
	uint32_t intr_stat;
	uint32_t intr_mask;
	uint32_t raw_intr_stat;
	uint32_t rx_thresh;
	uint32_t tx_thresh;
	uint32_t clear_intr;
	uint32_t clear_rx_under_intr;
	uint32_t clear_rx_over_intr;
	uint32_t clear_tx_over_intr;
	uint32_t clear_rd_req_intr;
	uint32_t clear_tx_abrt_intr;
	uint32_t clear_rx_done_intr;
	uint32_t clear_activity_intr;
	uint32_t clear_stop_det_intr;
	uint32_t clear_start_det_intr;
	uint32_t clear_gen_call_intr;
	uint32_t enable;
	uint32_t status;
	uint32_t tx_level;
	uint32_t rx_level;
	uint32_t sda_hold;
	uint32_t tx_abort_source;
} __attribute__((packed)) DesignwareI2cRegs;

/* High and low times in different speed modes (in ns). */
enum {
	MIN_SS_SCL_HIGHTIME = 4000,
	MIN_SS_SCL_LOWTIME = 4700,
	MIN_FS_SCL_HIGHTIME = 600,
	MIN_FS_SCL_LOWTIME = 1300,
	MIN_HS_SCL_HIGHTIME = 60,
	MIN_HS_SCL_LOWTIME = 160,
};

/* Speed mode thresholds. */
enum {
	MAX_SPEED_HZ = 3400000,
	FAST_SPEED_HZ = 400000,
	STANDARD_SPEED_HZ = 100000,

	CLK_MHZ = 166,
};

/* Control register definitions. */
enum {
	CONTROL_SD = 0x0040,
	CONTROL_RE = 0x0020,
	CONTROL_10BITADDRMASTER = 0x0010,
	CONTROL_10BITADDR_SLAVE = 0x0008,
	CONTROL_SPEED_MASK = 0x0006,
	CONTROL_SPEED_SS = 0x0002,
	CONTROL_SPEED_FS = 0x0004,
	CONTROL_SPEED_HS = 0x0006,
	CONTROL_MM = 0x0001,
};

/* cmd_data register definitions. */
enum {
	CMD_DATA_CMD = 0x0100,
	CMD_DATA_STOP = 0x0200,
};

/* status register definitions. */
enum {
	STATUS_SA = 0x0040,
	STATUS_MA = 0x0020,
	STATUS_RFF = 0x0010,
	STATUS_RFNE = 0x0008,
	STATUS_TFE = 0x0004,
	STATUS_TFNF = 0x0002,
	STATUS_ACT = 0x0001,
};

/* enable register definitions. */
enum {
	ENABLE_0B = 0x0001,
};

/* FIFO threshold register definitions. */
enum {
	FIFO_THRESH0 = 0x00,
	RX_THRESH = FIFO_THRESH0,
	TX_THRESH = FIFO_THRESH0,
};

/* Interrupt status register definitions. */
enum {
	INTR_GEN_CALL = 0x0800,
	INTR_START_DET = 0x0400,
	INTR_STOP_DET = 0x0200,
	INTR_ACTIVITY = 0x0100,
	INTR_RX_DONE = 0x0080,
	INTR_TX_ABRT = 0x0040,
	INTR_RD_REQ = 0x0020,
	INTR_TX_EMPTY = 0x0010,
	INTR_TX_OVER = 0x0008,
	INTR_RX_FULL = 0x0004,
	INTR_RX_OVER = 0x0002,
	INTR_RX_UNDER = 0x0001,
};

/*
 * set_speed_regs - Set bus speed controller registers.
 * @regs:	i2c register base address
 * @cntl_mask:	data to write to control register
 * @high_time:	high cycle time (ns)
 * @high_reg:	high cycle time register
 * @low_time:	low cycle time (ns)
 * @low_reg:	low cycle time register
 *
 * Set bus speed controller registers.
 */
static inline void set_speed_regs(DesignwareI2cRegs *regs, uint32_t cntl_mask,
				  int high_time, uint32_t *high_reg,
				  int low_time, uint32_t *low_reg)
{
	uint32_t cntl;

	writel(CLK_MHZ * high_time / 1000, high_reg);
	writel(CLK_MHZ * low_time / 1000, low_reg);

	cntl = (readl(&regs->control) & (~CONTROL_SPEED_MASK));
	cntl |= CONTROL_RE;
	writel(cntl | cntl_mask, &regs->control);
}

/*
 * i2c_set_bus_speed - Set the i2c speed.
 * @bus:	i2c bus description structure
 *
 * Set the i2c speed.
 */
static int i2c_set_bus_speed(DesignwareI2c *bus)
{
	DesignwareI2cRegs *regs = bus->regs;
	uint32_t enable;

	/* Disable controller before setting speed. */
	enable = readl(&regs->enable);
	enable &= ~ENABLE_0B;
	writel(enable, &regs->enable);

	if (bus->speed >= MAX_SPEED_HZ)
		set_speed_regs(regs, CONTROL_SPEED_HS,
			       MIN_HS_SCL_HIGHTIME, &regs->hs_scl_hcnt,
			       MIN_HS_SCL_LOWTIME, &regs->hs_scl_lcnt);
	else if (bus->speed >= FAST_SPEED_HZ)
		set_speed_regs(regs, CONTROL_SPEED_FS,
			       MIN_FS_SCL_HIGHTIME, &regs->fs_scl_hcnt,
			       MIN_FS_SCL_LOWTIME, &regs->fs_scl_lcnt);
	else
		set_speed_regs(regs, CONTROL_SPEED_SS,
			       MIN_SS_SCL_HIGHTIME, &regs->ss_scl_hcnt,
			       MIN_SS_SCL_LOWTIME, &regs->ss_scl_lcnt);

	/* Re-enable controller. */
	enable |= ENABLE_0B;
	writel(enable, &regs->enable);

	return 0;
}

/*
 * i2c_init - Init function.
 * @bus:	i2c bus description structure
 * @slaveadd:	slave address for controller (not used if master-only)
 *
 * Initialization function.
 */
static void i2c_init(DesignwareI2c *bus)
{
	DesignwareI2cRegs *regs = bus->regs;
	uint32_t enable;

	/* Disable controller. */
	enable = readl(&regs->enable);
	enable &= ~ENABLE_0B;
	writel(enable, &regs->enable);

	writel(CONTROL_SD | CONTROL_SPEED_FS | CONTROL_MM, &regs->control);
	writel(RX_THRESH, &regs->rx_thresh);
	writel(TX_THRESH, &regs->tx_thresh);
	i2c_set_bus_speed(bus);
	writel(INTR_STOP_DET, &regs->intr_mask);

	/* Re-enable controller. */
	enable = readl(&regs->enable);
	enable |= ENABLE_0B;
	writel(enable, &regs->enable);

	bus->initialized = 1;
}

/*
 * i2c_flush_rxfifo - Flushes the i2c RX FIFO.
 * @regs:	i2c register base address
 *
 * Flushes the i2c RX FIFO.
 */
static void i2c_flush_rxfifo(DesignwareI2cRegs *regs)
{
	while (readl(&regs->status) & STATUS_RFNE)
		readl(&regs->cmd_data);
}

/*
 * i2c_wait_for_bus_idle - Waits for idle bus.
 * @regs:	i2c register base address
 *
 * Waits for bus idle.
 */
static int i2c_wait_for_bus_idle(DesignwareI2cRegs *regs)
{
	uint64_t start = timer_us(0);

	while ((readl(&regs->status) & STATUS_MA) ||
	       !(readl(&regs->status) & STATUS_TFE))
		/* Evaluate timeout, wait for up to 16 bytes in FIFO. */
		if (timer_us(start) > 2000 * 16)
			return -1;

	return 0;
}

/*
 * i2c_xfer_finish - Complete an i2c transfer.
 * @regs:	i2c register base address
 *
 * Complete an i2c transfer.
 */
static int i2c_xfer_finish(DesignwareI2cRegs *regs)
{
	uint64_t start = timer_us(0);

	while (1) {
		if ((readl(&regs->raw_intr_stat) & INTR_STOP_DET)) {
			readl(&regs->clear_stop_det_intr);
			break;
		} else if (timer_us(start) > 2000)
			break;
	}

	if (i2c_wait_for_bus_idle(regs)) {
		printf("Timed out waiting for bus.\n");
		return -1;
	}

	i2c_flush_rxfifo(regs);

	/* TODO(shawnn): I removed a udelay(10000) here because the purpose was
	 * unclear. It's possible that we actually need to delay for some
	 * reason. Remove this comment once we are confident that things
	 * are working reliably.*/

	return 0;
}

/*
 * i2c_transfer_segment - Read / Write single segment.
 * @regs:	i2c register base address
 * @segment:	pointer to single segment
 * @send_stop:	true if stop condition should be sent at conclusion
 *
 * Read / Write single segment.
 */
static int i2c_transfer_segment(DesignwareI2cRegs *regs,
				I2cSeg *segment,
				int send_stop)
{
	int i;

	/* Set slave device bus address. */
	writel(segment->chip, &regs->target_addr);

	/* Read or write each byte in segment. */
	for (i = 0; i < segment->len; ++i) {
		uint64_t start = timer_us(0);
		uint32_t cmd;

		/* Write op only: Wait for FIFO not full. */
		if (!segment->read) {
			while (!(readl(&regs->status) & STATUS_TFNF))
				if (timer_us(start) > 2000)
					return -1;
			cmd = segment->buf[i];
		} else
			cmd = CMD_DATA_CMD;

		/* Send stop on last byte, if desired. */
		if (send_stop && i == segment->len - 1)
			cmd |= CMD_DATA_STOP;

		writel(cmd, &regs->cmd_data);

		/* Read op only: Wait FIFO data and store it. */
		if (segment->read) {
			while (!(readl(&regs->status) & STATUS_RFNE))
				if (timer_us(start) > 2000)
					return -1;
			segment->buf[i] = (uint8_t)readl(&regs->cmd_data);
		}
	}
	return 0;
}

/*
 * i2c_transfer - Read / Write from I2c registers.
 * @me:		i2c ops structure
 * @segments:	series of requested transactions
 * @seg_count:	no of segments
 *
 * Read / Write from i2c registers.
 */
static int i2c_transfer(I2cOps *me, I2cSeg *segments, int seg_count)
{
	int i;
	DesignwareI2c *bus = container_of(me, DesignwareI2c, ops);
	DesignwareI2cRegs *regs = bus->regs;

	if (!bus->initialized)
		i2c_init(bus);

	if (i2c_wait_for_bus_idle(regs))
		return -1;

	// Set stop condition on final segment only. Repeated start will
	// be automatically generated on R->W or W->R switch.
	for (i = 0; i < seg_count; ++i)
		if (i2c_transfer_segment(regs,
					 &segments[i],
					 i == seg_count - 1))
			return -1;

	return i2c_xfer_finish(regs);
}

/*
 * new_designware_i2c - Allocate new i2c bus.
 * @regs:	i2c register base address
 * @speed:	required i2c speed
 *
 * Allocate new designware i2c bus.
 */
DesignwareI2c *new_designware_i2c(uintptr_t reg_addr, int speed)
{
	DesignwareI2c *bus = xzalloc(sizeof(*bus));

	bus->ops.transfer = &i2c_transfer;
	bus->regs = (void *)reg_addr;
	bus->speed = speed;

	return bus;
}
