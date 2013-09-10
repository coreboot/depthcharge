/*
 * This file is part of the depthcharge project.
 *
 * (C) Copyright 2002
 * David Mueller, ELSOFT AG, d.mueller@elsoft.ch
 * Copyright 2013 Google Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <libpayload.h>

#include "base/container_of.h"
#include "base/time.h"
#include "drivers/bus/i2c/exynos5_usi.h"

typedef struct __attribute__ ((packed)) UsiI2cRegs {
	uint32_t usi_ctl;
	uint32_t usi_fifo_ctl;
	uint32_t usi_trailing_ctl;
	uint32_t usi_clk_ctl;
	uint32_t usi_clk_slot;
	uint32_t spi_ctl;
	uint32_t uart_ctl;
	uint32_t res1;
	uint32_t usi_int_en;
	uint32_t usi_int_stat;
	uint32_t modem_stat;
	uint32_t error_stat;
	uint32_t usi_fifo_stat;
	uint32_t usi_txdata;
	uint32_t usi_rxdata;
	uint32_t res2;
	uint32_t i2c_conf;
	uint32_t i2c_auto_conf;
	uint32_t i2c_timeout;
	uint32_t i2c_manual_cmd;
	uint32_t i2c_trans_status;
	uint32_t i2c_timing_hs1;
	uint32_t i2c_timing_hs2;
	uint32_t i2c_timing_hs3;
	uint32_t i2c_timing_fs1;
	uint32_t i2c_timing_fs2;
	uint32_t i2c_timing_fs3;
	uint32_t i2c_timing_sla;
	uint32_t i2c_addr;
} UsiI2cRegs;

// I2C_CTL
enum {
	HSI2C_FUNC_MODE_I2C = 1 << 0,
	HSI2C_MASTER = 1 << 3,
	HSI2C_RXCHON = 1 << 6,
	HSI2C_TXCHON = 1 << 7,
	HSI2C_SW_RST = 1 << 31
};

// I2C_FIFO_STAT
enum {
	HSI2C_TX_FIFO_LEVEL = 0x7f << 0,
	HSI2C_TX_FIFO_FULL = 1 << 7,
	HSI2C_TX_FIFO_EMPTY = 1 << 8,
	HSI2C_RX_FIFO_LEVEL = 0x7f << 16,
	HSI2C_RX_FIFO_FULL = 1 << 23,
	HSI2C_RX_FIFO_EMPTY = 1 << 24
};

// I2C_FIFO_CTL
enum {
	HSI2C_RXFIFO_EN = 1 << 0,
	HSI2C_TXFIFO_EN = 1 << 1,
	HSI2C_TXFIFO_TRIGGER_LEVEL = 0x20 << 16,
	HSI2C_RXFIFO_TRIGGER_LEVEL = 0x20 << 4
};

// I2C_TRAILING_CTL
enum {
	HSI2C_TRAILING_COUNT = 0xff
};

// I2C_INT_EN
enum {
	HSI2C_INT_TX_ALMOSTEMPTY_EN = 1 << 0,
	HSI2C_INT_RX_ALMOSTFULL_EN = 1 << 1,
	HSI2C_INT_TRAILING_EN = 1 << 6,
	HSI2C_INT_I2C_EN = 1 << 9
};

// I2C_CONF
enum {
	HSI2C_AUTO_MODE = 1 << 31,
	HSI2C_10BIT_ADDR_MODE = 1 << 30,
	HSI2C_HS_MODE = 1 << 29
};

// I2C_AUTO_CONF
enum {
	HSI2C_READ_WRITE = 1 << 16,
	HSI2C_STOP_AFTER_TRANS = 1 << 17,
	HSI2C_MASTER_RUN = 1 << 31
};

// I2C_TIMEOUT
enum {
	HSI2C_TIMEOUT_EN = 1 << 31
};

// I2C_TRANS_STATUS
enum {
	HSI2C_MASTER_BUSY = 1 << 17,
	HSI2C_SLAVE_BUSY = 1 << 16,
	HSI2C_TIMEOUT_AUTO = 1 << 4,
	HSI2C_NO_DEV = 1 << 3,
	HSI2C_NO_DEV_ACK = 1 << 2,
	HSI2C_TRANS_ABORT = 1 << 1,
	HSI2C_TRANS_DONE = 1 << 0
};

#define HSI2C_SLV_ADDR_MAS(x)		((x & 0x3ff) << 10)

enum {
	HSI2C_TIMEOUT = 100
};

static int usi_i2c_get_clk_details(UsiI2cRegs *regs, int *div, int *cycle,
				   unsigned op_clk)
{
	// Assume the input clock is 66MHz for now.
	unsigned int clkin = 66666666;

	/*
	 * FPCLK / FI2C =
	 * (CLK_DIV + 1) * (TSCLK_L + TSCLK_H + 2) + 8 + 2 * FLT_CYCLE
	 * temp0 = (CLK_DIV + 1) * (TSCLK_L + TSCLK_H + 2)
	 * temp1 = (TSCLK_L + TSCLK_H + 2)
	 */
	uint32_t flt_cycle = (readl(&regs->i2c_conf) >> 16) & 0x7;
	int temp = (clkin / op_clk) - 8 - 2 * flt_cycle;

	// CLK_DIV max is 256.
	for (int i = 0; i < 256; i++) {
		int period = temp / (i + 1) - 2;
		if (period < 512 && period >= 2) {
			*cycle = period;
			*div = i;
			return 0;
		}
	}
	printf("%s: Failed to find timing parameters.\n", __func__);
	return -1;
}

static void exynos5_usi_i2c_ch_init(UsiI2cRegs *regs, unsigned frequency)
{
	int div, cycle;
	if (usi_i2c_get_clk_details(regs, &div, &cycle, frequency))
		return;

	uint32_t sr_release;
	sr_release = cycle;

	uint32_t scl_l, scl_h, start_su, start_hd, stop_su;
	scl_l = scl_h = start_su = start_hd = stop_su = cycle / 2;

	uint32_t data_su, data_hd;
	data_su = data_hd = cycle / 4;

	uint32_t timing_fs1 = start_su << 24 | start_hd << 16 | stop_su << 8;
	uint32_t timing_fs2 = data_su << 24 | scl_l << 8 | scl_h << 0;
	uint32_t timing_fs3 = div << 16 | sr_release << 0;
	uint32_t timing_sla = data_hd << 0;

	// Currently operating in fast speed mode.
	writel(timing_fs1, &regs->i2c_timing_fs1);
	writel(timing_fs2, &regs->i2c_timing_fs2);
	writel(timing_fs3, &regs->i2c_timing_fs3);
	writel(timing_sla, &regs->i2c_timing_sla);

	// Clear to enable timeout.
	writel(readl(&regs->i2c_timeout) & ~HSI2C_TIMEOUT_EN,
	       &regs->i2c_timeout);

	writel(HSI2C_TRAILING_COUNT, &regs->usi_trailing_ctl);
	writel(HSI2C_RXFIFO_EN | HSI2C_TXFIFO_EN, &regs->usi_fifo_ctl);
	writel(readl(&regs->i2c_conf) | HSI2C_AUTO_MODE, &regs->i2c_conf);
}

static void exynos5_usi_i2c_reset(UsiI2cRegs *regs, unsigned frequency)
{
	// Set and clear the bit for reset.
	writel(readl(&regs->usi_ctl) | HSI2C_SW_RST, &regs->usi_ctl);
	writel(readl(&regs->usi_ctl) & ~HSI2C_SW_RST, &regs->usi_ctl);

	exynos5_usi_i2c_ch_init(regs, frequency);
}

/*
 * Check whether the transfer is complete.
 * Return values:
 * 0  - transfer not done
 * 1  - transfer finished successfully
 * -1 - transfer failed
 */
static int exynos5_usi_i2c_check_transfer(UsiI2cRegs *regs)
{
	uint32_t status = readl(&regs->i2c_trans_status);
	if (status & (HSI2C_TRANS_ABORT | HSI2C_NO_DEV_ACK |
		      HSI2C_NO_DEV | HSI2C_TIMEOUT_AUTO)) {
		if (status & HSI2C_TRANS_ABORT)
			printf("%s: Transaction aborted.\n", __func__);
		if (status & HSI2C_NO_DEV_ACK)
			printf("%s: No ack from device.\n", __func__);
		if (status & HSI2C_NO_DEV)
			printf("%s: No response from device.\n", __func__);
		if (status & HSI2C_TIMEOUT_AUTO)
			printf("%s: Transaction time out.\n", __func__);
		return -1;
	}
	return !(status & HSI2C_MASTER_BUSY);
}

/*
 * Wait for the transfer to finish.
 * Return values:
 * 0  - transfer not done
 * 1  - transfer finished successfully
 * -1 - transfer failed
 */
static int exynos5_usi_i2c_wait_for_transfer(UsiI2cRegs *regs)
{
	uint64_t start = timer_us(0);
	while (timer_us(start) < HSI2C_TIMEOUT * 1000) {
		int ret = exynos5_usi_i2c_check_transfer(regs);
		if (ret)
			return ret;
	}
	return 0;
}

static int exynos5_usi_i2c_senddata(UsiI2cRegs *regs, uint8_t *data, int len)
{
	while (!exynos5_usi_i2c_check_transfer(regs) && len) {
		if (!(readl(&regs->usi_fifo_stat) & HSI2C_TX_FIFO_FULL)) {
			writel(*data++, &regs->usi_txdata);
			len--;
		}
	}
	return len ? -1 : 0;
}

static int exynos5_usi_i2c_recvdata(UsiI2cRegs *regs, uint8_t *data, int len)
{
	while (!exynos5_usi_i2c_check_transfer(regs) && len) {
		if (!(readl(&regs->usi_fifo_stat) & HSI2C_RX_FIFO_EMPTY)) {
			*data++ = readl(&regs->usi_rxdata);
			len--;
		}
	}
	return len ? -1 : 0;
}

static int exynos5_usi_i2c_write(I2cOps *me, uint8_t chip,
				 uint32_t addr, int addr_len,
				 uint8_t *data, int data_len)
{
	Exynos5UsiI2c *bus = container_of(me, Exynos5UsiI2c, ops);
	if (!bus)
		return -1;
	UsiI2cRegs *regs = bus->reg_addr;

	if (!bus->ready) {
		exynos5_usi_i2c_reset(regs, bus->frequency);
		bus->ready = 1;
	}

	if (exynos5_usi_i2c_wait_for_transfer(regs) != 1) {
		exynos5_usi_i2c_reset(regs, bus->frequency);
		return -1;
	}

	writel(HSI2C_SLV_ADDR_MAS(chip), &regs->i2c_addr);
	writel(HSI2C_TXCHON | HSI2C_FUNC_MODE_I2C | HSI2C_MASTER,
	       &regs->usi_ctl);
	int len = data_len + addr_len;
	writel(len | HSI2C_STOP_AFTER_TRANS | HSI2C_MASTER_RUN,
	       &regs->i2c_auto_conf);

	if (addr_len) {
		uint8_t addr_buf[sizeof(addr)];
		for (int i = 0; i < addr_len; i++) {
			int byte = addr_len - 1 - i;
			addr_buf[i] = addr >> (byte * 8);
		}

		if (exynos5_usi_i2c_senddata(regs, addr_buf, addr_len)) {
			exynos5_usi_i2c_reset(regs, bus->frequency);
			return -1;
		}
	}

	if (exynos5_usi_i2c_senddata(regs, data, data_len) ||
	    exynos5_usi_i2c_wait_for_transfer(regs) != 1) {
		exynos5_usi_i2c_reset(regs, bus->frequency);
		return -1;
	}

	writel(HSI2C_FUNC_MODE_I2C, &regs->usi_ctl);
	return 0;
}

static int exynos5_usi_i2c_read(I2cOps *me, uint8_t chip,
				uint32_t addr, int addr_len,
				uint8_t *data, int data_len)
{
	Exynos5UsiI2c *bus = container_of(me, Exynos5UsiI2c, ops);
	if (!bus)
		return -1;
	UsiI2cRegs *regs = bus->reg_addr;

	if (!bus->ready) {
		exynos5_usi_i2c_reset(regs, bus->frequency);
		bus->ready = 1;
	}

	if (exynos5_usi_i2c_wait_for_transfer(regs) != 1) {
		exynos5_usi_i2c_reset(regs, bus->frequency);
		return -1;
	}

	writel(HSI2C_SLV_ADDR_MAS(chip), &regs->i2c_addr);

	if (addr_len) {
		uint8_t addr_buf[sizeof(addr)];
		for (int i = 0; i < addr_len; i++) {
			int byte = addr_len - 1 - i;
			addr_buf[i] = addr >> (byte * 8);
		}

		writel(HSI2C_TXCHON | HSI2C_FUNC_MODE_I2C | HSI2C_MASTER,
		       &regs->usi_ctl);
		writel(addr_len | HSI2C_MASTER_RUN | HSI2C_STOP_AFTER_TRANS,
		       &regs->i2c_auto_conf);

		if (exynos5_usi_i2c_senddata(regs, addr_buf, addr_len) ||
		    exynos5_usi_i2c_wait_for_transfer(regs) != 1) {
			exynos5_usi_i2c_reset(regs, bus->frequency);
			return -1;
		}
	}

	writel(HSI2C_RXCHON | HSI2C_FUNC_MODE_I2C | HSI2C_MASTER,
	       &regs->usi_ctl);
	writel(data_len | HSI2C_STOP_AFTER_TRANS |
	       HSI2C_READ_WRITE | HSI2C_MASTER_RUN, &regs->i2c_auto_conf);

	if (exynos5_usi_i2c_recvdata(regs, data, data_len) ||
	    exynos5_usi_i2c_wait_for_transfer(regs) != 1) {
		exynos5_usi_i2c_reset(regs, bus->frequency);
		return -1;
	}

	writel(HSI2C_FUNC_MODE_I2C, &regs->usi_ctl);
	return 0;
}

Exynos5UsiI2c *new_exynos5_usi_i2c(void *reg_addr, unsigned frequency)
{
	Exynos5UsiI2c *bus = malloc(sizeof(*bus));
	if (!bus) {
		printf("Failed to allocate I2C bus structure.\n");
		return NULL;
	}
	memset(bus, 0, sizeof(*bus));

	bus->ops.read = &exynos5_usi_i2c_read;
	bus->ops.write = &exynos5_usi_i2c_write;
	bus->reg_addr = reg_addr;
	bus->frequency = frequency;
	return bus;
}
