/*
 * (C) Copyright 2009
 * Vipin Kumar, ST Micoelectronics, vipin.kumar@st.com.
 * Copyright 2013 Google LLC
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
 */

#include <assert.h>
#include <libpayload.h>

#include "base/container_of.h"
#include "drivers/bus/i2c/designware.h"

#define DESIGNWARE_I2C_DEBUG 0

typedef struct {
	uint32_t control;		// 0x00
	uint32_t target_addr;		// 0x04
	uint32_t slave_addr;		// 0x08
	uint32_t master_addr;		// 0x0c
	uint32_t cmd_data;		// 0x10
	uint32_t ss_scl_hcnt;		// 0x14
	uint32_t ss_scl_lcnt;		// 0x18
	uint32_t fs_scl_hcnt;		// 0x1c
	uint32_t fs_scl_lcnt;		// 0x20
	uint32_t hs_scl_hcnt;		// 0x24
	uint32_t hs_scl_lcnt;		// 0x28
	uint32_t intr_stat;		// 0x2c
	uint32_t intr_mask;		// 0x30
	uint32_t raw_intr_stat;		// 0x34
	uint32_t rx_thresh;		// 0x38
	uint32_t tx_thresh;		// 0x3c
	uint32_t clear_intr;		// 0x40
	uint32_t clear_rx_under_intr;	// 0x44
	uint32_t clear_rx_over_intr;	// 0x48
	uint32_t clear_tx_over_intr;	// 0x4c
	uint32_t clear_rd_req_intr;	// 0x50
	uint32_t clear_tx_abrt_intr;	// 0x54
	uint32_t clear_rx_done_intr;	// 0x58
	uint32_t clear_activity_intr;	// 0x5c
	uint32_t clear_stop_det_intr;	// 0x60
	uint32_t clear_start_det_intr;	// 0x64
	uint32_t clear_gen_call_intr;	// 0x68
	uint32_t enable;		// 0x6c
	uint32_t status;		// 0x70
	uint32_t tx_fifo_level;		// 0x74
	uint32_t rx_fifo_level;		// 0x78
	uint32_t sda_hold;		// 0x7c
	uint32_t tx_abort_source;	// 0x80
	uint32_t slv_data_nak_only;	// 0x84
	uint32_t dma_cr;		// 0x88
	uint32_t dma_tdlr;		// 0x8c
	uint32_t dma_rdlr;		// 0x90
	uint32_t sda_setup;		// 0x94
	uint32_t ack_general_call;	// 0x98
	uint32_t enable_status;		// 0x9c
	uint32_t fs_spklen;		// 0xa0
	uint32_t hs_spklen;		// 0xa4
	uint32_t clr_restart_det;	// 0xa8
	uint32_t comp_param1;		// 0xac
	uint32_t comp_version;		// 0xb0
	uint32_t comp_type;		// 0xb4
} DesignwareI2cRegs;

_Static_assert(offsetof(DesignwareI2cRegs, comp_type) == 0xb4,
	"DesignwareI2cRegs misaligned");

#define TX_ABORT_SOURCE_TX_FLUSH_CNT_SHIFT 24
#define TX_ABORT_SOURCE_TX_FLUSH_CNT_MASK                                      \
	(0xFF << TX_ABORT_SOURCE_TX_FLUSH_CNT_SHIFT)
#define TX_ABORT_SOURCE_RESERVED_SHIFT 17
#define TX_ABORT_SOURCE_RESERVED_MASK (0x7f << TX_ABORT_SOURCE_RESERVED_SHIFT)

/* High and low times in different speed modes (in ns). */
enum {
	DEFAULT_SDA_HOLD_TIME = 300,
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
	/* Slave FSM Activity */
	STATUS_SA = 0x0040,
	/* Master FSM Activity */
	STATUS_MA = 0x0020,
	/* Receive FIFO Completely Full */
	STATUS_RFF = 0x0010,
	/* Receive FIFO Not Empty */
	STATUS_RFNE = 0x0008,
	/* Transmit FIFO Completely Empty */
	STATUS_TFE = 0x0004,
	/* Transmit FIFO Not Full */
	STATUS_TFNF = 0x0002,
	/* Activity Status */
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
	/* A General Call address is received and acknowledged */
	INTR_GEN_CALL = 0x0800,
	/* Either a START or RESTART condition occurred on the I2C interface */
	INTR_START_DET = 0x0400,
	/* A STOP condition occurred on the I2C interface */
	INTR_STOP_DET = 0x0200,
	/* There was I2C activity */
	INTR_ACTIVITY = 0x0100,
	/* This occurs on the last byte of the transmission, indicating that the
	transmission is done */
	INTR_RX_DONE = 0x0080,
	/* The I2C controller is unable to complete the intended actions on the
	   contents of the transmit FIFO */
	INTR_TX_ABRT = 0x0040,
	/* The I2C controller is acting as a slave and an I2C master is
	   attempting to read data from the controller */
	INTR_RD_REQ = 0x0020,
	/* The transmit buffer is at or below the threshold value */
	INTR_TX_EMPTY = 0x0010,
	/* During transmit the transmit buffer is filled to TxBufferDepth */
	INTR_TX_OVER = 0x0008,
	/* The receive buffer reaches or goes above the receive threshold */
	INTR_RX_FULL = 0x0004,
	/* The receive buffer is completely filled to RxBufferDepth */
	INTR_RX_OVER = 0x0002,
	/* The processor attempts to read the receive buffer when it is empty */
	INTR_RX_UNDER = 0x0001,
};

enum {
	TIMEOUT_US = 10000
};

/* Abort reasons for register tx_abort_source. */
static const char *const tx_abort_reason[] = {
	"Abrt7BAddrNoAck",
	"Abrt10Addr1NoAck",
	"Abrt10Addr2NoAck",
	"AbrtTxDataNoAck",
	"AbrtGCallNoAck",
	"AbrtGCallRead",
	"AbrtHsAckDet",
	"AbrtSbyteAckDet",
	"AbrtHsNoRstrt",
	"AbrtSByteNoRstrt",
	"Abrt10BRdNoRstrt",
	"AbrtMasterDis",
	"ArbLost",
	"AbrtSlvFlushTxFifo",
	"AbrtSlvArbLost",
	"AbrtSlvRdInTx",
	"ABRT_USER_ABRT",
};

/*
 * print_tx_aborted - Pretty prints the tx_abort_source register.
 * @regs:	i2c register base address
 **/
static void print_tx_aborted(DesignwareI2cRegs *regs)
{
	uint32_t tx_abrt = read32(&regs->tx_abort_source);

	printf("i2c transaction aborted:");
	for (size_t i = 0; i < ARRAY_SIZE(tx_abort_reason); ++i) {
		unsigned int mask = (1U << i);
		if (tx_abrt & mask)
			printf(" [%#x:%s]", mask, tx_abort_reason[i]);
	}

	uint32_t tx_fifo_level =
		(tx_abrt & TX_ABORT_SOURCE_TX_FLUSH_CNT_MASK) >>
		TX_ABORT_SOURCE_TX_FLUSH_CNT_SHIFT;

	if (tx_fifo_level)
		printf(" [%#x: TX FIFO Level]", tx_fifo_level);

	uint32_t reserved = (tx_abrt & TX_ABORT_SOURCE_RESERVED_MASK) >>
			    TX_ABORT_SOURCE_RESERVED_SHIFT;

	if (reserved)
		printf(" [%#x:Reserved]", reserved);

	printf("\n");
}

/*
 * i2c_print_status_registers - Prints controller status registers
 * @regs:	i2c register base address
 */
static void i2c_print_status_registers(DesignwareI2cRegs *regs)
{
	printf("Status: %#x, Interrupt Status: %#x\n"
	       "RX FIFO Level: %u, TX FIFO Level: %u\n"
	       "Enable: %#x, Enable Status: %#x\n"
	       "Abort Source: %#x\n",
	       read32(&regs->status), read32(&regs->raw_intr_stat),
	       read32(&regs->rx_fifo_level), read32(&regs->tx_fifo_level),
	       read32(&regs->enable), read32(&regs->enable_status),
	       read32(&regs->tx_abort_source));
}

static void i2c_enable(DesignwareI2cRegs *regs)
{
	if (!(read32(&regs->enable) & ENABLE_0B))
		write32(&regs->enable, ENABLE_0B);
	else
		printf("%s: i2s bus is already enabled.\n", __func__ );
}

static int i2c_disable(DesignwareI2cRegs *regs)
{
	if (read32(&regs->enable) & ENABLE_0B) {
		uint64_t start;

		write32(&regs->enable, 0);

		/* Wait for enable status bit to clear */
		start = timer_us(0);
		while (read32(&regs->enable_status) & ENABLE_0B) {
			if (timer_us(start) > TIMEOUT_US) {
				printf("%s: Timeout\n", __func__);
				i2c_print_status_registers(regs);
				return -1;
			}
		}
	} else {
		printf("%s: i2s bus is already disabled.\n", __func__ );
	}

	return 0;
}

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
static inline void set_speed_regs(DesignwareI2c *bus, uint32_t cntl_mask,
				  int high_time, uint32_t *high_reg,
				  int low_time, uint32_t *low_reg)
{
	DesignwareI2cRegs *regs = bus->regs;
	uint32_t cntl;

	write32(high_reg, bus->clk_mhz * high_time / 1000);
	write32(low_reg, bus->clk_mhz * low_time / 1000);
	write32(&regs->sda_hold, bus->clk_mhz * DEFAULT_SDA_HOLD_TIME / 1000);

	cntl = (read32(&regs->control) & (~CONTROL_SPEED_MASK));
	cntl |= CONTROL_RE;
	write32(&regs->control, cntl | cntl_mask);
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

	if (bus->speed >= MAX_SPEED_HZ)
		set_speed_regs(bus, CONTROL_SPEED_HS,
			       MIN_HS_SCL_HIGHTIME, &regs->hs_scl_hcnt,
			       MIN_HS_SCL_LOWTIME, &regs->hs_scl_lcnt);
	else if (bus->speed >= FAST_SPEED_HZ)
		set_speed_regs(bus, CONTROL_SPEED_FS,
			       MIN_FS_SCL_HIGHTIME, &regs->fs_scl_hcnt,
			       MIN_FS_SCL_LOWTIME, &regs->fs_scl_lcnt);
	else
		set_speed_regs(bus, CONTROL_SPEED_SS,
			       MIN_SS_SCL_HIGHTIME, &regs->ss_scl_hcnt,
			       MIN_SS_SCL_LOWTIME, &regs->ss_scl_lcnt);

	return 0;
}

/*
 * i2c_speed_init_done - Check if bus speed is already configured
 * @high_reg:	high cycle time register
 * @low_reg:	low cycle time register
 *
 * Check if high and low cycle time registers are configured.
 *
 */
static int i2c_speed_init_done(uint32_t *high_reg, uint32_t *low_reg)
{
	/*
	 * If both high_reg and low_reg are set to non-zero value, assume that
	 * the bus speed is already configured.
	 */
	return read32(high_reg) && read32(low_reg);
}

/*
 * i2c_bus_initialized - Check if coreboot already initialized bus
 * @bus:	i2c bus description structure
 *
 * Check if coreboot already initializaed the bus.
 *
 */
static int i2c_bus_initialized(DesignwareI2c *bus)
{
	DesignwareI2cRegs *regs = bus->regs;

	if (bus->speed >= MAX_SPEED_HZ)
		return i2c_speed_init_done(&regs->hs_scl_hcnt,
						&regs->hs_scl_lcnt);
	else if (bus->speed >= FAST_SPEED_HZ)
		return i2c_speed_init_done(&regs->fs_scl_hcnt,
						&regs->fs_scl_lcnt);

	return i2c_speed_init_done(&regs->ss_scl_hcnt,
					&regs->ss_scl_lcnt);
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

	/*
	 * If bus is already initialized in coreboot, skip initialization here
	 * and set bus->initialized to 1 directly.
	 */
	if (i2c_bus_initialized(bus)) {
		bus->initialized = 1;
		return;
	}

	/* Disable controller. */
	i2c_disable(regs);

	write32(&regs->control, CONTROL_SD | CONTROL_SPEED_FS | CONTROL_MM);
	write32(&regs->rx_thresh, RX_THRESH);
	write32(&regs->tx_thresh, TX_THRESH);
	i2c_set_bus_speed(bus);
	write32(&regs->intr_mask, INTR_STOP_DET);

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
	unsigned int cnt = 0;

	while (read32(&regs->status) & STATUS_RFNE) {
		cnt++;
		read32(&regs->cmd_data);
	}

	if (cnt)
		printf("%s: %u bytes were flushed from the rx fifo!\n",
		       __func__, cnt);
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

	do {
		unsigned int status = read32(&regs->status);

		if (!(status & STATUS_MA) && (status & STATUS_TFE))
			return 0;

	} while (timer_us(start) <= TIMEOUT_US * 16);

	printf("%s: timeout waiting for bus to become idle.\n", __func__);

	i2c_print_status_registers(regs);

	return -1;
}

/*
 * process_fatal_interrupts - Processes fatal interrupts..
 * @regs:	i2c register base address
 *
 * Returns 1 if a fatal interrupt has occurred, otherwise 0.
 */
static int process_fatal_interrupts(DesignwareI2cRegs *regs)
{
	unsigned int intr_stat = read32(&regs->raw_intr_stat);

	int ret = 0;

	if ((intr_stat & INTR_TX_ABRT)) {
		print_tx_aborted(regs);

		read32(&regs->clear_tx_abrt_intr);
		ret = 1;
	}

	if ((intr_stat & INTR_RX_UNDER)) {
		printf("%s: RX FIFO Underflow!\n", __func__);
		read32(&regs->clear_rx_under_intr);
		ret = 1;
	}

	if ((intr_stat & INTR_RX_OVER)) {
		printf("%s: RX FIFO Overflow!\n", __func__);
		read32(&regs->clear_rx_over_intr);
		ret = 1;
	}

	if ((intr_stat & INTR_TX_OVER)) {
		printf("%s: TX FIFO Overflow!\n", __func__);
		read32(&regs->clear_tx_over_intr);
		ret = 1;
	}

	return ret;
}

/*
 * Waits TIMEOUT_US for a stop condition on the bus.
 *
 * Returns -1 on failure and 0 on success.
 */
static int i2c_wait_for_stop_condition(DesignwareI2cRegs *regs)
{
	uint64_t start = timer_us(0);

	do {
		if (process_fatal_interrupts(regs)) {
			printf("%s: Fatal Interrupt\n", __func__);
			return -1;
		}

		if (read32(&regs->raw_intr_stat) & INTR_STOP_DET)
			return 0;

	} while (timer_us(start) <= TIMEOUT_US);

	printf("%s: timed out\n", __func__);

	i2c_print_status_registers(regs);

	return -1;
}

/*
 * Waits TIMEOUT_US for space in the the TX fifo.
 *
 * Returns -1 on failure and 0 on success.
 */
static int i2c_wait_for_tx_fifo_not_full(DesignwareI2cRegs *regs)
{
	uint64_t start = timer_us(0);

	do {
		if (process_fatal_interrupts(regs)) {
			printf("%s: Fatal Interrupt\n", __func__);
			return -1;
		}

		if (read32(&regs->status) & STATUS_TFNF)
			return 0;

	} while (timer_us(start) <= TIMEOUT_US);

	printf("%s: timed out waiting for space in the transmit fifo\n",
	       __func__);

	i2c_print_status_registers(regs);

	return -1;
}

/*
 * Waits TIMEOUT_US for a byte in the the RX fifo.
 *
 * Returns -1 on failure and 0 on success.
 */
static int i2c_wait_for_rx_fifo_not_empty(DesignwareI2cRegs *regs)
{
	uint64_t start = timer_us(0);

	do {
		if (process_fatal_interrupts(regs)) {
			printf("%s: Fatal Interrupt\n", __func__);
			return -1;
		}

		if (read32(&regs->status) & STATUS_RFNE)
			return 0;

	} while (timer_us(start) <= TIMEOUT_US);

	printf("%s: timed out waiting for byte to arrive in the receive FIFO\n",
	       __func__);

	i2c_print_status_registers(regs);

	return -1;
}

/*
 * i2c_xfer_finish - Complete an i2c transfer.
 * @regs:	i2c register base address
 *
 * Complete an i2c transfer.
 */
static int i2c_xfer_finish(DesignwareI2cRegs *regs)
{
	if (i2c_wait_for_stop_condition(regs)) {
		printf("%s: Error occurred while waiting for stop condition.\n",
		       __func__);
		return -1;
	}

	if (i2c_wait_for_bus_idle(regs)) {
		printf("%s: Error occurred while waiting for bus to become "
		       "idle.\n",
		       __func__);
		return -1;
	}

	/* Why do we do this? There should be no need to flush the RX FIFO. */
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

	/* Read or write each byte in segment. */
	for (i = 0; i < segment->len; ++i) {
		uint32_t cmd = 0;

		/* Send stop on last byte, if desired. */
		if (send_stop && i == segment->len - 1)
			cmd |= CMD_DATA_STOP;

		if (segment->read) {
			cmd |= CMD_DATA_CMD;

			write32(&regs->cmd_data, cmd);

			if (i2c_wait_for_rx_fifo_not_empty(regs))
				goto err;

			segment->buf[i] = (uint8_t) read32(&regs->cmd_data);
		} else {
			cmd |= segment->buf[i];

			if (i2c_wait_for_tx_fifo_not_full(regs))
				goto err;

			write32(&regs->cmd_data, cmd);
		}
	}

	return 0;
 err:
	printf("%s: Transferring byte %u of %d failed.\n", __func__, i,
	       segment->len);
	return -1;
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
	int i, ret = -1;
	DesignwareI2c *bus = container_of(me, DesignwareI2c, ops);
	DesignwareI2cRegs *regs = bus->regs;
	uint8_t last_tar;

	if (!bus->initialized)
		i2c_init(bus);

	/* Set target address first, while i2c is still disabled. */
	last_tar = segments[0].chip;
	write32(&regs->target_addr, last_tar);

	i2c_enable(regs);

	if (i2c_wait_for_bus_idle(regs)) {
		printf("%s: failed to start transfer.\n", __func__);
		goto out;
	}

	// Set stop condition on final segment only. Repeated start will
	// be automatically generated on R->W or W->R switch.
	for (i = 0; i < seg_count; ++i) {
		if (DESIGNWARE_I2C_DEBUG)
			printf("i2c %02x %s %d bytes : ", segments[i].chip,
			       segments[i].read ? "R" : "W", segments[i].len);

		/*
		 * Designware IP Target Address Register (TAR) can only be
		 * updated while i2c is disabled.
		 *
		 * However, disabling and re-enabling i2c between segments
		 * means a repeated-start cannot used sent, which is useful
		 * especially when doing a simple i2c register read.
		 *
		 * So, only do the disable-TAR-enable sequence if the TAR has
		 * changed between segments.
		 */
		if (segments[i].chip != last_tar) {
			if (DESIGNWARE_I2C_DEBUG)
				printf("updating tar from %#x to %#x\n",
				       last_tar, segments[i].chip);
			i2c_disable(regs);
			/* Target address must be set when i2c is disabled. */
			write32(&regs->target_addr, segments[i].chip);
			last_tar = segments[i].chip;
			i2c_enable(regs);
		}

		if (i2c_transfer_segment(regs,
					 &segments[i],
					 i == seg_count - 1)) {
			printf("%s: transferring segment %d of %d failed.\n",
			       __func__, i, seg_count);
			goto out;
		}

		if (DESIGNWARE_I2C_DEBUG) {
			int j;
			for (j = 0; j < segments[i].len; j++)
				printf("%02x ", segments[i].buf[j]);
			printf("\n");
		}
	}

	ret = i2c_xfer_finish(regs);
 out:
	read32(&regs->clear_intr);
	i2c_disable(regs);
	return ret;
}

/*
 * new_designware_i2c - Allocate new i2c bus.
 * @regs:	i2c register base address
 * @speed:	required i2c speed
 * @clk_mhz:	controller core clock speed in MHz
 *
 * Allocate new designware i2c bus.
 */
DesignwareI2c *new_designware_i2c(uintptr_t reg_addr, int speed, int clk_mhz)
{
	DesignwareI2c *bus = xzalloc(sizeof(*bus));

	bus->ops.transfer = &i2c_transfer;
	bus->regs = (void *)reg_addr;
	bus->speed = speed;
	bus->clk_mhz = clk_mhz;

	if (CONFIG(CLI))
		add_i2c_controller_to_list(&bus->ops, "Designware-%08x",
					   (uint32_t)reg_addr);

	return bus;
}
