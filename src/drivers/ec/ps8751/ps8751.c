/*
 * Copyright 2017 Google Inc.
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

#include <endian.h>
#include <libpayload.h>
#include <vb2_api.h>

#include "base/container_of.h"
#include "base/init_funcs.h"
#include "drivers/ec/ps8751/ps8751.h"

#define PS8751_DEBUG	0

#if (PS8751_DEBUG > 0)
#define debug(msg, args...)	printf("%s: " msg, __func__, ##args)
#else
#define debug(msg, args...)
#endif

#ifndef __must_check
#define __must_check	__attribute__((warn_unused_result))
#endif

#define PAGE_0		(0x10 >> 1)
#define PAGE_1		(0x12 >> 1)
#define PAGE_2		(0x14 >> 1)
#define PAGE_3		(0x16 >> 1)	/* primary TCPCI registers */

#define P0_ROM_CTRL		0xef
#define P0_ROM_CTRL_LOAD_DONE	0xc0	/* MTP load done */

#define P1_CHIP_REV_LO		0xf0	/* the 0x03 in "A3" */
#define P1_CHIP_REV_HI		0xf1	/* the 0x0a in "A3" */
#define P1_CHIP_ID_LO		0xf2	/* 0x51, etc. */
#define P1_CHIP_ID_HI		0xf3	/* 0x87, 0x88 */

#define PS8751_P1_SPI_WP	0x4b
/* NOTE: on 8751 A3 silicon, P1_SPI_WP_EN reads back inverted! */
#define PS8751_P1_SPI_WP_EN	0x10	/* WP enable bit */
#define PS8751_P1_SPI_WP_DIS	0x00	/* WP disable "bit" */

#define PS8805_P2_SPI_WP	0x2a
#define PS8805_P2_SPI_WP_EN	0x10	/* WP enable bit */
#define PS8805_P2_SPI_WP_DIS	0x00	/* WP disable "bit" */

#define PS8815_P2_SPI_WP	0x2a
#define PS8815_P2_SPI_WP_EN	0x00	/* WP enable "bit" */
#define PS8815_P2_SPI_WP_DIS	0x10	/* WP disable bit */

#define P2_ALERT_LOW		0x10
#define P2_ALERT_HIGH		0x11
#define P2_WR_FIFO		0x90
#define P2_RD_FIFO		0x91
#define P2_SPI_LEN		0x92
#define P2_SPI_CTRL		0x93
#define P2_SPI_CTRL_NOREAD	0x04
#define P2_SPI_CTRL_FIFO_RESET	0x02
#define P2_SPI_CTRL_TRIGGER	0x01
#define P2_SPI_STATUS		0x9e
#define P2_CLK_CTRL		0xd6

#define P3_VENDOR_ID_LOW	0x00
#define P3_CHIP_WAKEUP		0xa0

#define PS8751_P3_I2C_DEBUG		0xa0
#define PS8751_P3_I2C_DEBUG_DEFAULT	0x31
#define PS8751_P3_I2C_DEBUG_ENABLE	0x30
#define PS8751_P3_I2C_DEBUG_DISABLE	PS8751_P3_I2C_DEBUG_DEFAULT

#define PS8815_P3_I2C_DEBUG		0x9b
#define PS8815_P3_I2C_DEBUG_DEFAULT	0x00
#define PS8815_P3_I2C_DEBUG_ENABLE	0x01
#define PS8815_P3_I2C_DEBUG_DISABLE	0x02

/*
 * bytes of SPI FIFO depth after command overhead
 */

#define PS_FW_RD_CHUNK		16
#define PS_FW_WR_CHUNK		12

#define SPI_CMD_WRITE_STATUS_REG	0x01
#define SPI_CMD_PROG_PAGE		0x02
#define SPI_CMD_READ_DATA		0x03
#define SPI_CMD_WRITE_DISABLE		0x04
#define SPI_CMD_READ_STATUS_REG		0x05
#define SPI_CMD_WRITE_ENABLE		0x06

/*
 * EN25F20:
 *	  64 x   4KB erase sectors
 *	   4 x  64KB erase blocks
 *	1024 x 256B  write pages
 */

#define SPI_CMD_ERASE_SECTOR	0x20		/* sector erase, 4KB */
#define SPI_CMD_READ_DEVICE_ID	0x90
#define SPI_PAGE_SIZE		(1 << 8)	/* 256B, always 2^n */
#define SPI_PAGE_MASK		(SPI_PAGE_SIZE - 1)

#define SPI_STATUS_WIP		0x01
#define SPI_STATUS_WEL		0x02
#define SPI_STATUS_BP		0x0c
#define SPI_STATUS_SRP		0x80

/*
 * period of issuing reads on the primary I2C page to keep the chip awake
 * 1 sec is OK
 * 2 secs is too long
 * chip goes to sleep when I2C is idle for 2 secs
 */

#define USEC_TO_SEC(us)		((us) / 1000000)
#define USEC_TO_MSEC(us)	((us) / 1000)
#define PS_REFRESH_INTERVAL_US	(500 * 1000)		/* 500 ms */
#define PS_SPI_TIMEOUT_US	(1 * 1000 * 1000)	/* 1s */
#define PS_WIP_TIMEOUT_US	(1 * 1000 * 1000)	/* 1s */
#define PS_MPU_BOOT_DELAY_MS	(50)
#define PS_RESTART_DELAY_CS	(6)			/* 6cs / 60 ms */

#define PARADE_BINVERSION_OFFSET	0x501c
#define PARADE_CHIPVERSION_OFFSET	0x503a

#if (PS8751_DEBUG >= 2)
#define PARADE_FW_START		0x38000
#else
#define PARADE_FW_START		0x30000
#endif
#define PARADE_FW_END		0x40000
#define PARADE_FW_SECTOR	 0x1000		/* erase sector size */

#define PARADE_TEST_FW_SIZE	0x1000

#define PARADE_VENDOR_ID		0x1DA0
#define PARADE_PS8751_PRODUCT_ID	0x8751
#define PARADE_PS8755_PRODUCT_ID	0x8755
#define PARADE_PS8705_PRODUCT_ID	0x8705
/* TODO(b/171422252): Remove the default device ID */
#define PARADE_PS8705_DEFAULT_DEVICE_ID	0x0001
#define PARADE_PS8705_A2_DEVICE_ID	0x0004
#define PARADE_PS8705_A3_DEVICE_ID	0x0005
#define PARADE_PS8805_BROKEN_PRODUCT_ID	0x8803
#define PARADE_PS8805_PRODUCT_ID	0x8805
#define PARADE_PS8805_A2_DEVICE_ID	0x0001
#define PARADE_PS8805_A3_DEVICE_ID	0x0002
#define PARADE_PS8815_PRODUCT_ID	0x8815
#define PARADE_PS8815_A0_DEVICE_ID	0x0001
#define PARADE_PS8815_A1_DEVICE_ID	0x0002
#define PARADE_PS8815_A2_DEVICE_ID	0x0003

enum ps8751_device_state {
	PS8751_DEVICE_MISSING = -2,
	PS8751_DEVICE_NOT_PARADE = -1,
	PS8751_DEVICE_PRESENT = 0,
};

static const uint8_t erased_bytes[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};
_Static_assert(sizeof(erased_bytes) == PS_FW_RD_CHUNK,
	       "erased_bytes initializer size mismatch");

/**
 * write a single byte to a register of an i2c page
 *
 * @param me	device context
 * @param page	i2c device address
 * @param reg	i2c register on target device
 * @param data	byte to write to register
 * @return 0 if ok, -1 on error
 */

static int __must_check write_reg(Ps8751 *me,
				  uint8_t page, uint8_t reg, uint8_t data)
{
	return i2c_writeb(&me->bus->ops, page, reg, data);
}

/**
 * issue a series of i2c writes
 *
 * @param me	device context
 * @param page	i2c device address
 * @param cmds	vector of i2c reg write commands
 * @param count	number of elements in ops vector
 * @return 0 if ok, -1 on error
 */

static int __must_check write_regs(Ps8751 *me, uint8_t page,
				   const I2cWriteVec *cmds, const size_t count)
{
	return i2c_write_regs(&me->bus->ops, page, cmds, count);
}

/**
 * read a single byte from a register of a SPI target
 *
 * @param me	device context
 * @param page	i2c device address
 * @param reg	i2c register on target device
 * @param data	pointer to return byte
 * @return 0 if ok, -1 on error
 */

static int __must_check read_reg(Ps8751 *me,
				 uint8_t page, uint8_t reg, uint8_t *data)
{
	return i2c_readb(&me->bus->ops, page, reg, data);
}

/**
 * issue a series of i2c reads
 *
 * @param me	device context
 * @param regs	vector of i2c regs to read read
 * @param count	number of elements in regs array
 * @param data	pointer to read bytes
 * @return 0 if ok, -1 on error
 */

static int __must_check read_regs(Ps8751 *me,
				  uint8_t chip,
				  const uint8_t *regs,
				  const size_t count,
				  uint8_t *data)
{
	return i2c_read_regs(&me->bus->ops, chip, regs, count, data);
}

/**
 * wait for SPI interface FIFOs to become ready
 * this means the requested command bytes have been written and
 * result bytes have been captured
 *
 * SPI bus timeout can happen if the SPI CLK isn't
 * running as expected.
 *
 * @param me	device context
 * @return 0 if ok, -1 on error
 */

static int __must_check ps8751_spi_fifo_wait_busy(Ps8751 *me)
{
	uint8_t status;
	uint64_t t0_us;

	t0_us = timer_us(0);
	do {
		if (read_reg(me, PAGE_2, P2_SPI_CTRL, &status) != 0)
			return -1;
		if (timer_us(t0_us) >= PS_SPI_TIMEOUT_US) {
			printf("%s: SPI bus timeout after %ums\n",
			       me->chip_name, USEC_TO_MSEC(PS_SPI_TIMEOUT_US));
			return -1;
		}
	} while (status & P2_SPI_CTRL_TRIGGER);
	return 0;
}

/**
 * reset the SPI interface FIFOs
 *
 * @param me	device context
 * @return 0 if ok, -1 on error
 */

static int __must_check ps8751_spi_fifo_reset(Ps8751 *me)
{
	if (write_reg(me, PAGE_2, P2_SPI_CTRL, P2_SPI_CTRL_FIFO_RESET) != 0)
		return -1;
	return 0;
}

/**
 * wake up the chip and enable all the i2c targets (i.e. "pages")
 * - also reset SPI interface FIFOs for good measure
 *
 * @param me	device context
 * @return 0 if ok, -1 on error
 */

static int __must_check ps8751_wake_i2c(Ps8751 *me)
{
	int status;
	uint8_t dummy;
	uint8_t debug_reg;
	uint8_t debug_ena;

	debug("call...\n");

	switch (me->chip_type) {
	case CHIP_PS8751:
	case CHIP_PS8755:
	case CHIP_PS8705:
	case CHIP_PS8805:
		debug_reg = PS8751_P3_I2C_DEBUG;
		debug_ena = PS8751_P3_I2C_DEBUG_ENABLE;
		break;
	case CHIP_PS8815:
		debug_reg = PS8815_P3_I2C_DEBUG;
		debug_ena = PS8815_P3_I2C_DEBUG_ENABLE;
		break;
	default:
		printf("Unknown Parade chip_type: %d\n", me->chip_type);
		return -1;
	}

	status = read_reg(me, PAGE_3, P3_CHIP_WAKEUP, &dummy);
	if (status != 0) {
		/* wait for device to wake up... */
		mdelay(10);
	}

	/*
	 * this enables 7 additional i2c chip addrs:
	 * 0x10, 0x12, 0x14, 0x18, 0x1a, 0x1c, 0x1f
	 */
	status = write_reg(me, PAGE_3, debug_reg, debug_ena);
	if (status == 0)
		status = ps8751_spi_fifo_reset(me);
	if (status != 0)
		printf("%s: chip did not wake up!\n", me->chip_name);
	return status;
}

/**
 * turn off extra i2c targets (pages)
 *
 * @param me	device context
 * @return 0 if ok, -1 on error
 */

static int __must_check ps8751_hide_i2c(Ps8751 *me)
{
	int status;
	uint8_t dummy;
	uint8_t debug_reg;
	uint8_t debug_dis;

	switch (me->chip_type) {
	case CHIP_PS8751:
	case CHIP_PS8755:
	case CHIP_PS8705:
	case CHIP_PS8805:
		debug_reg = PS8751_P3_I2C_DEBUG;
		debug_dis = PS8751_P3_I2C_DEBUG_DEFAULT;
		break;
	case CHIP_PS8815:
		debug_reg = PS8815_P3_I2C_DEBUG;
		debug_dis = PS8815_P3_I2C_DEBUG_DEFAULT;
		break;
	default:
		printf("Unknown Parade chip_type: %d\n", me->chip_type);
		return -1;
	}

	/* make sure chip is awake (is this needed?) */
	status = read_reg(me, PAGE_3, P3_CHIP_WAKEUP, &dummy);
	if (status != 0) {
		/* wait for device to wake up... */
		mdelay(10);
	}
	status = write_reg(me, PAGE_3, debug_reg, debug_dis);
	return status;
}

/**
 * clear the ps8751 TCPCI alerts.  after the MPU has been stopped, the
 * TCPCI regs on page 0 (0x10) need to be used instead of page 3
 * (0x16).
 *
 * @param me	device context
 * @return 0 if ok, -1 on error
 */

static int __must_check ps8751_clear_alerts(Ps8751 *me)
{
	const I2cWriteVec am[] = {
		{ P2_ALERT_LOW, 0xff },
		{ P2_ALERT_HIGH, 0xff },
	};
	/* yes, page 0 */
	return write_regs(me, PAGE_0, am, ARRAY_SIZE(am));
}

/*
 * protect internal registers
 *
 * this is a special step suggested by parade to protect
 * some internal SPI registers to improve the odds of
 * reflashing a chip with bad firmware.
 */

static int __must_check ps8751_rom_ctrl(Ps8751 *me)
{
	if (me->chip_type == CHIP_PS8751) {
		/* the ps8751 does not support this register */
		return 0;
	}

	/* mark MTP load done */
	if (write_reg(me, PAGE_0, P0_ROM_CTRL, P0_ROM_CTRL_LOAD_DONE) != 0)
		return -1;
	return 0;
}

/*
 * stop the MPU, reset SPI clock
 *
 * if the MPU isn't running properly (i.e. corrupted flash),
 * we need to disable both clocks, then re-enable the SPI clock
 * to be able to access the SPI FIFO to recover the chip.
 *
 * ? disabling the MPU may keep the chip awake!
 */

static int __must_check ps8751_disable_mpu(Ps8751 *me)
{
	/* turn off SPI|MPU clocks */
	if (write_reg(me, PAGE_2, P2_CLK_CTRL, 0xc0) != 0)
		return -1;
	/* SPI clock on, MPU clock stays off */
	if (write_reg(me, PAGE_2, P2_CLK_CTRL, 0x40) != 0)
		return -1;
	/* clear residual alerts */
	if (ps8751_clear_alerts(me) != 0)
		return -1;
	return 0;
}

/**
 * re-enable the MPU clock and reboot it
 *
 * SPI clock stays on
 * we tune MUX_DP_EQ_CONFIGURATION when initializing the chip from the ec
 * parade confirmed this tuning is preserved across this reboot
 *
 * the chip needs about 50ms to come out of reset, so we'll assume it
 * takes a similar amount of time for the MPU to boot up.
 *
 * @param me	device context
 * @return 0 if ok, -1 on error
 */

static int __must_check ps8751_enable_mpu(Ps8751 *me)
{
	/* SPI|MPU clk on */
	if (write_reg(me, PAGE_2, P2_CLK_CTRL, 0x00) != 0)
		return -1;
	mdelay(PS_MPU_BOOT_DELAY_MS);
	if (ps8751_wake_i2c(me) != 0)
		return -1;
	return 0;
}

/**
 * send a SPI write-enable cmd
 *
 * WRITE_ENABLE must be issued before every
 * PROGRAM, ERASE, WRITE_STATUS_REGISTER command
 *
 * @param me	device context
 * @return 0 if ok, -1 on error
 */

static int __must_check ps8751_spi_cmd_enable_writes(Ps8751 *me)
{
	static const I2cWriteVec we[] = {
		{ P2_WR_FIFO, SPI_CMD_WRITE_ENABLE },
		{ P2_SPI_LEN, 0x00 },
		{ P2_SPI_CTRL, P2_SPI_CTRL_NOREAD|P2_SPI_CTRL_TRIGGER },
	};

	if (write_regs(me, PAGE_2, we, ARRAY_SIZE(we)) != 0)
		return -1;
	if (ps8751_spi_fifo_wait_busy(me) != 0)
		return -1;
	return 0;
}

/*
 * wait for "erase/program command finished" as seen by the ps8751
 */

static int __must_check ps8751_spi_wait_prog_cmd(Ps8751 *me)
{
	uint8_t busy;
	uint64_t t0_us;

	t0_us = timer_us(0);
	do {
		if (read_reg(me, PAGE_2, P2_SPI_STATUS, &busy) != 0)
			return -1;
		if ((busy & 0x3f) == 0x00) {
			/* {chip,sector} erase, program cmd finished */
			return 0;
		}
	} while (timer_us(t0_us) < PS_WIP_TIMEOUT_US);
	printf("%s: flash prog/erase timeout after %ums\n",
	       me->chip_name, USEC_TO_MSEC(PS_SPI_TIMEOUT_US));
	return -1;
}

/**
 * read SPI flash status register
 *
 * @param me	device context
 * @return status if ok, -1 on error
 */

static int __must_check ps8751_spi_cmd_read_status(Ps8751 *me, uint8_t *status)
{
	static const I2cWriteVec rs[] = {
		{ P2_WR_FIFO, SPI_CMD_READ_STATUS_REG },
		{ P2_SPI_LEN, 0x00 },
		{ P2_SPI_CTRL, P2_SPI_CTRL_TRIGGER },
	};

	if (write_regs(me, PAGE_2, rs, ARRAY_SIZE(rs)) != 0)
		return -1;
	if (ps8751_spi_fifo_wait_busy(me) != 0)
		return -1;
	if (read_reg(me, PAGE_2, P2_RD_FIFO, status) != 0)
		return -1;
	return 0;
}

/**
 * wait for SPI flash status WIP (write-in-progress) bit to clear
 * this is needed after write-status-reg, any prog, any erase command
 *
 * @param me	device context
 * @return status if ok, -1 on error
 */

static int __must_check ps8751_spi_wait_wip(Ps8751 *me)
{
	uint8_t status;
	uint64_t t0_us;

	t0_us = timer_us(0);
	do {
		if (ps8751_spi_cmd_read_status(me, &status) != 0)
			return -1;
		if (timer_us(t0_us) >= PS_WIP_TIMEOUT_US) {
			printf("%s: WIP timeout after %ums\n",
			       me->chip_name, USEC_TO_MSEC(PS_WIP_TIMEOUT_US));
			return -1;
		}
	} while (status & SPI_STATUS_WIP);
	return 0;
}

/**
 * write SPI flash status register
 *
 * @param me	device context
 * @return 0 if ok, -1 on error
 */

static int __must_check ps8751_spi_cmd_write_status(Ps8751 *me, uint8_t val)
{
	if (ps8751_spi_cmd_enable_writes(me) < 0)
		return -1;

	const I2cWriteVec ws[] = {
		{ P2_WR_FIFO, SPI_CMD_WRITE_STATUS_REG },
		{ P2_WR_FIFO, val },
		{ P2_SPI_LEN, 0x01 },
		{ P2_SPI_CTRL, P2_SPI_CTRL_NOREAD|P2_SPI_CTRL_TRIGGER },
	};

	if (write_regs(me, PAGE_2, ws, ARRAY_SIZE(ws)) != 0)
		return -1;
	if (ps8751_spi_fifo_wait_busy(me) != 0)
		return -1;
	if (ps8751_spi_wait_wip(me) != 0)
		return -1;
	return 0;
}

/*
 * lock the SPI flash
 *
 * @param me	device context
 * @return 0 if ok, -1 on error
 *
 * NOTE: keep in sync with ps8751_spi_flash_unlock()
 */

static int __must_check ps8751_spi_flash_lock(Ps8751 *me)
{
	int status = 0;
	uint8_t page;
	uint8_t wp_reg;
	uint8_t wp_en;

	if (ps8751_spi_cmd_write_status(me, SPI_STATUS_SRP|SPI_STATUS_BP) != 0)
		status = -1;

	switch (me->chip_type) {
	case CHIP_PS8751:
		page = PAGE_1;
		wp_reg = PS8751_P1_SPI_WP;
		wp_en = PS8751_P1_SPI_WP_EN;
		break;
	case CHIP_PS8705:
	case CHIP_PS8755:
	case CHIP_PS8805:
		page = PAGE_2;
		wp_reg = PS8805_P2_SPI_WP;
		wp_en = PS8805_P2_SPI_WP_EN;
		break;
	case CHIP_PS8815:
		page = PAGE_2;
		wp_reg = PS8815_P2_SPI_WP;
		wp_en = PS8815_P2_SPI_WP_EN;
		break;
	default:
		printf("Unknown Parade chip_type: %d\n", me->chip_type);
		return -1;
	}
	/* assert SPI flash WP# */
	if (write_reg(me, page, wp_reg, wp_en) != 0)
		status = -1;

	return status;
}

/*
 * reset SPI bus cmd FIFOs and unlock the SPI flash...
 *
 * @param me	device context
 *
 * NOTE: call ps8751_disable_mpu() before this so we have
 *	 a functional SPI bus.
 */

static int __must_check ps8751_spi_flash_unlock(Ps8751 *me)
{
	uint8_t status;
	uint8_t page;
	uint8_t wp_reg;
	uint8_t wp_dis;

	if (ps8751_spi_fifo_reset(me) != 0)
		return -1;

	switch (me->chip_type) {
	case CHIP_PS8751:
		page = PAGE_1;
		wp_reg = PS8751_P1_SPI_WP;
		wp_dis = PS8751_P1_SPI_WP_DIS;
		break;
	case CHIP_PS8705:
	case CHIP_PS8755:
	case CHIP_PS8805:
		page = PAGE_2;
		wp_reg = PS8805_P2_SPI_WP;
		wp_dis = PS8805_P2_SPI_WP_DIS;
		break;
	case CHIP_PS8815:
		page = PAGE_2;
		wp_reg = PS8815_P2_SPI_WP;
		wp_dis = PS8815_P2_SPI_WP_DIS;
		break;
	default:
		printf("Unknown Parade chip_type: %d\n", me->chip_type);
		return -1;
	}
	/* deassert SPI flash WP# */
	if (write_reg(me, page, wp_reg, wp_dis) != 0)
		return -1;

	/* clear the SRP, BP bits */
	if (ps8751_spi_cmd_write_status(me, 0x00) != 0)
		return -1;

	if (ps8751_spi_cmd_read_status(me, &status) != 0)
		return -1;

	if ((status & (SPI_STATUS_SRP|SPI_STATUS_BP)) != 0) {
		printf("%s: could not clear flash status "
		       "SRP|BP (0x%02x)\n", me->chip_name, status);
		return -1;
	}
	return 0;
}

/*
 * wait for "erase/program command finished" as seen by the ps8751
 * then, wait for the WIP (write-in-progress) bit to clear on the
 * flash part itself.
 *
 * @param me	device context
 */

static int __must_check ps8751_spi_wait_rom_ready(Ps8751 *me)
{
	if (ps8751_spi_wait_prog_cmd(me) != 0)
		return -1;
	if (ps8751_spi_wait_wip(me) != 0)
		return -1;
	return 0;
}

/**
 * query the flash ID and see if we support it
 *
 * @param me	device context
 * @return 0 if ok, -1 on error
 */

static int __must_check ps8751_spi_flash_identify(Ps8751 *me)
{
	uint8_t buf[2] = {0, 0};
	uint16_t flash_id;

	static const I2cWriteVec read_id[] = {
		{ P2_WR_FIFO, SPI_CMD_READ_DEVICE_ID },
		{ P2_WR_FIFO, 0x00 },
		{ P2_WR_FIFO, 0x00 },
		{ P2_WR_FIFO, 0x00 },
		{ P2_SPI_LEN, 0x13 },
		{ P2_SPI_CTRL, P2_SPI_CTRL_TRIGGER },
	};
	if (write_regs(me, PAGE_2, read_id, ARRAY_SIZE(read_id)) != 0)
		return -1;
	if (ps8751_spi_fifo_wait_busy(me) != 0)
		return -1;

	static const uint8_t get_id[ARRAY_SIZE(buf)] = {
		P2_RD_FIFO,
		P2_RD_FIFO,
	};
	if (read_regs(me, PAGE_2, get_id, ARRAY_SIZE(buf), buf) != 0)
		return -1;
	flash_id = be16dec(buf);

	printf("%s: found SPI flash ID 0x%04x\n", me->chip_name, flash_id);

	/*
	 * these devices must use
	 * SPI_CMD_ERASE_SECTOR of 0x20 with a 4KB erase block
	 * list extracted from parade reference code
	 * (PS8751_PanelConfig.py)
	 */
	switch (flash_id) {
	case 0x1c11:		/* "EN25F20" */
	case 0x1c12:		/* "EN25F40" */
	case 0xc811:		/* "GD25Q20C" */
	case 0xbf43:		/* "25LF020A" */
	case 0xef11:		/* "W25X20" */
	case 0xef15:		/* "W25X32" */
	case 0xc211:		/* "25L2005" */
	case 0xc205:		/* "25L512" */
	case 0x1f44:		/* "25DF041A" */
	case 0x1f43:		/* "25DF021A" */
		break;
	default:
		printf("%s: SPI flash ID 0x%04x not recognized\n",
		       me->chip_name, flash_id);
		return -1;
	}
	return 0;
}

/**
 * access the chip periodically so it doesn't fall asleep.  a simple
 * i2c read every second or so from PAGE_3 is sufficient.
 *
 * @param me		device context
 * @param deadline	pointer to time of next needed access
 *			use deadline of 0 for first call
 * @return 0 if ok, -1 on error
 */

static int __must_check ps8751_keep_awake(Ps8751 *me, uint64_t *deadline)
{
	uint64_t now_us = timer_us(0);
	uint8_t dummy;

	if (now_us >= *deadline) {
		*deadline = now_us + PS_REFRESH_INTERVAL_US;
		int status = read_reg(me, PAGE_3, P3_CHIP_WAKEUP, &dummy);
		if (status != 0) {
			printf("%s: chip dozed off!\n", me->chip_name);
			return -1;
		}
	}
	return 0;
}

/**
 * get chip hardware version.  result of 0xa3 means it's an A3 chip.
 *
 * @param me		device context
 * @param version	pointer to result version byte
 * @return 0 if ok, -1 on error
 */

static int __must_check ps8751_get_hw_version(Ps8751 *me, uint8_t *version)
{
	int status;
	uint8_t low;
	uint8_t high;

	status = read_reg(me, PAGE_1, P1_CHIP_REV_LO, &low);
	if (status == 0)
		status = read_reg(me, PAGE_1, P1_CHIP_REV_HI, &high);
	if (status < 0) {
		printf("%s: read P1_CHIP_REV_* failed\n", me->chip_name);
		return status;
	}
	*version = (high << 4) | low;
	return 0;
}

static int is_corrupted_tcpc(const struct ec_response_pd_chip_info *const info,
			     const enum ParadeChipType chip)
{
	switch (chip) {
	case CHIP_PS8751:
		return info->vendor_id == 0 && info->product_id == 0;
	case CHIP_PS8705:
	case CHIP_PS8755:
	case CHIP_PS8805:
		return info->vendor_id == PARADE_VENDOR_ID &&
		       info->product_id == PARADE_PS8805_BROKEN_PRODUCT_ID;
	default:
		return 0;
	}
}

static int is_parade_chip(const struct ec_response_pd_chip_info *const info,
			  const enum ParadeChipType chip)
{
	if (info->vendor_id != PARADE_VENDOR_ID)
		return 0;

	switch (chip) {
	case CHIP_PS8751:
		return info->product_id == PARADE_PS8751_PRODUCT_ID;
	case CHIP_PS8755:
		return info->product_id == PARADE_PS8755_PRODUCT_ID;
	case CHIP_PS8705:
		return info->product_id == PARADE_PS8705_PRODUCT_ID;
	case CHIP_PS8805:
		return info->product_id == PARADE_PS8805_PRODUCT_ID;
	case CHIP_PS8815:
		return info->product_id == PARADE_PS8815_PRODUCT_ID;
	default:
		printf("Unknown Parade product_id: 0x%x\n", info->product_id);
		return 0;
	}
}

/**
 * capture chip (vendor, product, device, rev) IDs
 *
 * @param me	device context
 * @return 0 if ok, -1 on error
 */

static enum ps8751_device_state __must_check ps8751_capture_device_id(
							Ps8751 *me, int renew)
{
	struct ec_params_pd_chip_info p;
	struct ec_response_pd_chip_info r;

	if (me->chip.vendor != 0 && !renew)
		return PS8751_DEVICE_PRESENT;

	p.port = me->ec_pd_id;
	p.live = renew;
	int status = ec_command(me->bus->ec, EC_CMD_PD_CHIP_INFO, 0,
				&p, sizeof(p), &r, sizeof(r));
	if (status < 0) {
		printf("%s: could not get chip info!\n", me->chip_name);
		return PS8751_DEVICE_MISSING;
	}

	uint16_t vendor  = r.vendor_id;
	uint16_t product = r.product_id;
	uint16_t device  = r.device_id;
	uint8_t fw_rev = r.fw_version_number;

	printf("%s: vendor 0x%04x product 0x%04x "
	       "device 0x%04x fw_rev 0x%02x\n",
	       me->chip_name, vendor, product, device, fw_rev);
	if (is_corrupted_tcpc(&r, me->chip_type)) {
		printf("%s: reports corruption (%4x:%4x)\n",
		       me->chip_name, vendor, product);
	} else if (!is_parade_chip(&r, me->chip_type)) {
		return PS8751_DEVICE_NOT_PARADE;
	}

	me->chip.vendor = vendor;
	me->chip.product = product;
	me->chip.device = device;
	me->chip.fw_rev = fw_rev;
	return PS8751_DEVICE_PRESENT;
}

/**
 * extract the chip version compatibility info from a firmware blob
 *
 * @fw_blob	firmware blob
 * @return chip version (e.g. 0xa3 is an A3 chip)
 */

static uint8_t __must_check ps8751_blob_hw_version(const uint8_t *fw_blob)
{
	char buf[3];

	buf[0] = fw_blob[PARADE_CHIPVERSION_OFFSET];
	buf[1] = fw_blob[PARADE_CHIPVERSION_OFFSET + 1];
	buf[2] = '\0';
	return strtol(buf, 0, 16);
}

/**
 * determine if a firmware blob is compatible with the hardware
 *
 * @param me	device context
 * @param fw	proposed firmware blob
 * @return compatibility status (boolean)
 */

static int __must_check ps8751_is_fw_compatible(Ps8751 *me, const uint8_t *fw)
{
	uint8_t hw_rev;
	uint8_t fw_chip_version;

	if (ps8751_get_hw_version(me, &hw_rev) < 0)
		return 0;

	switch (me->chip_type) {
	case CHIP_PS8751:
		fw_chip_version = ps8751_blob_hw_version(fw);
		break;
	case CHIP_PS8705:
	case CHIP_PS8755:
	case CHIP_PS8805:
		fw_chip_version = me->blob_hw_version;
		break;
	case CHIP_PS8815:
		fw_chip_version = me->blob_hw_version;
		break;
	default:
		printf("Unknown Parade chip_type: %d\n", me->chip_type);
		return 0;
	}
	if (hw_rev == fw_chip_version)
		return 1;
	printf("%s: chip rev 0x%02x but firmware for 0x%02x\n",
	       me->chip_name, hw_rev, fw_chip_version);
	return 0;
}

/**
 * write a SPI command plus a 24 bit addr to the SPI interface FIFO
 *
 * @param me	device context
 * @cmd		SPI command to issue
 * @param a24	SPI command address bits (24)
 * @return 0 if ok, -1 on error
 */

static int __must_check ps8751_spi_setup_cmd24(Ps8751 *me,
					       uint8_t cmd, uint32_t a24)
{
	const I2cWriteVec sa[] = {
		{ P2_WR_FIFO, cmd },
		{ P2_WR_FIFO, a24 >> 16 },
		{ P2_WR_FIFO, a24 >>  8 },
		{ P2_WR_FIFO, a24 },
	};
	return write_regs(me, PAGE_2, sa, ARRAY_SIZE(sa));
}

/**
 * issue a single flash sector erase command to
 * erase PARADE_FW_SECTOR (4KB) bytes.
 *
 * @param me		device context
 * @param offset	device byte offset, but containing
 *			sector is erased
 * @return 0 if ok, -1 on error
 */

static int __must_check ps8751_sector_erase(Ps8751 *me, uint32_t offset)
{
	if (ps8751_spi_cmd_enable_writes(me) != 0)
		return -1;
	if (ps8751_spi_setup_cmd24(me, SPI_CMD_ERASE_SECTOR, offset) != 0)
		return -1;

	static const I2cWriteVec se[] = {
		{ P2_SPI_LEN, 0x03 },
		{ P2_SPI_CTRL, P2_SPI_CTRL_NOREAD|P2_SPI_CTRL_TRIGGER },
	};
	if (write_regs(me, PAGE_2, se, ARRAY_SIZE(se)) != 0)
		return -1;
	if (ps8751_spi_fifo_wait_busy(me) != 0)
		return -1;
	if (ps8751_spi_wait_rom_ready(me) != 0)
		return -1;
	return 0;
}

/**
 * erase a chunk of flash.  offset and data_size must be sector aligned,
 * which is PARADE_FW_SECTOR (4KB).
 *
 * assumes SPI interface has been enabled for programming.
 *
 * @param me		device context
 * @param offset	device offset, 1st byte to erase
 * @param data_size	number of bytes to erase
 * @return 0 if ok, -1 on error
 */

static int __must_check ps8751_erase(Ps8751 *me,
				     uint32_t offset, uint32_t data_size)
{
	uint32_t end = offset + data_size;
	int rval = 0;
	uint64_t t0_us;

	debug("offset 0x%06x size %u\n", offset, data_size);

	t0_us = timer_us(0);
	for (; offset < end; offset += PARADE_FW_SECTOR) {
		if (ps8751_sector_erase(me, offset) != VB2_SUCCESS)
			rval = -1;
	}
	printf("%s: erased %uKB in %ums\n",
	       me->chip_name,
	       data_size >> 10,
	       (unsigned)USEC_TO_MSEC(timer_us(t0_us)));
	return rval;
}

/**
 * program flash with new data
 *
 * flash is assumed to be erased
 * the MPU is assumed to be stopped (highly recommended)
 * the SPI bus and flash are write-enabled
 *
 * @param me		device context
 * @param fw_start	flash device offset to program
 * @param data		addr of data to write
 * @param data_size	size of data to write
 * @return 0 if ok, -1 on error
 */

static int __must_check ps8751_program(Ps8751 *me,
				       const uint32_t fw_start,
				       const uint8_t * const data,
				       const int data_size)
{
	uint32_t data_offset;
	uint64_t t0_us;
	int chunk;
	int chunk_mods;
	int bytes_skipped = 0;

	printf("%s: programming %uKB...\n", me->chip_name, data_size >> 10);

	t0_us = timer_us(0);
	for (data_offset = 0;
	     data_offset < data_size;
	     data_offset += chunk) {
		chunk = MIN(PS_FW_WR_CHUNK, data_size - data_offset);
		/* clip at flash page boundary */
		int page_offset = (fw_start + data_offset) & SPI_PAGE_MASK;
		chunk = MIN(chunk, SPI_PAGE_SIZE - page_offset);

		/* any changes in this chunk? */
		chunk_mods = 0;
		for (int i = 0; i < chunk; ++i) {
			if (data[data_offset + i] != 0xff) {
				++chunk_mods;
				break;
			}
		}
		if (chunk_mods == 0) {
			bytes_skipped += chunk;
			continue;
		}

		if (ps8751_spi_cmd_enable_writes(me) != 0)
			return -1;

		if (ps8751_spi_setup_cmd24(me,
					   SPI_CMD_PROG_PAGE,
					   fw_start + data_offset) != 0) {
			return -1;
		}
		for (int i = 0; i < chunk; ++i) {
			if (write_reg(me, PAGE_2,
				      P2_WR_FIFO, data[data_offset + i]) != 0)
				return -1;
		}

		const I2cWriteVec wr[] = {
			{ P2_SPI_LEN, (4 + chunk - 1) },
			{ P2_SPI_CTRL,
			  P2_SPI_CTRL_NOREAD|P2_SPI_CTRL_TRIGGER },
		};
		if (write_regs(me, PAGE_2, wr, ARRAY_SIZE(wr)) != 0)
			return -1;
		if (ps8751_spi_fifo_wait_busy(me) != 0)
			return -1;
		if (ps8751_spi_wait_rom_ready(me) != 0)
			return -1;
	}
	printf("%s: programmed %uKB in %us (%uB skipped)\n",
	       me->chip_name,
	       data_size >> 10,
	       (unsigned int)USEC_TO_SEC(timer_us(t0_us)),
	       bytes_skipped);
	return 0;
}

/**
 * verify flash content matches given data
 *
 * @param me		device context
 * @param fw_start	flash device offset to verify
 * @param data		addr of data to match
 * @param data_size	size of data to match
 * @return 0 if ok, -1 on error
 */

static int __must_check ps8751_verify(Ps8751 *me,
				      const uint32_t fw_addr,
				      const uint8_t * const data,
				      const size_t data_size)
{
	uint64_t deadline = 0;
	uint8_t readback;
	uint64_t t0_us;
	uint32_t data_offset;
	int chunk;

	debug("offset 0x%06x size %zu\n", fw_addr, data_size);

	t0_us = timer_us(0);
	for (data_offset = 0;
	     data_offset < data_size;
	     data_offset += chunk) {
		chunk = MIN(PS_FW_RD_CHUNK, data_size - data_offset);

		if (ps8751_keep_awake(me, &deadline) != 0)
			return -1;
		if (ps8751_spi_setup_cmd24(me, SPI_CMD_READ_DATA,
					   fw_addr + data_offset) != 0) {
			return -1;
		}

		const I2cWriteVec rd[] = {
			{ P2_SPI_LEN,
			  ((chunk - 1) << 4) | (4 - 1) },
			{ P2_SPI_CTRL, P2_SPI_CTRL_TRIGGER },
		};
		if (write_regs(me, PAGE_2, rd, ARRAY_SIZE(rd)) != 0)
			return -1;
		if (ps8751_spi_fifo_wait_busy(me) != 0)
			return -1;
		for (int i = 0; i < chunk; ++i) {
			if (read_reg(me, PAGE_2, P2_RD_FIFO, &readback) != 0)
				return -1;
			if (readback != data[data_offset + i]) {
				printf("%s: mismatch at offset 0x%06x "
				       "0x%02x != 0x%02x (expected)\n",
				       me->chip_name,
				       fw_addr + data_offset + i,
				       readback, data[data_offset + i]);
				return -1;
			}
		}
	}
	printf("%s: verified %zuKB in %us\n",
	       me->chip_name,
	       data_size >> 10,
	       (unsigned)USEC_TO_SEC(timer_us(t0_us)));
	return 0;
}

static int ps8751_construct_i2c_tunnel(Ps8751 *me)
{
	int ret;
	struct ec_response_locate_chip r;

	ret = cros_ec_locate_tcpc_chip(me->ec_pd_id, &r);
	if (ret)
		return ret;

	if (r.bus_type != EC_BUS_TYPE_I2C) {
		printf("%s: Unexpected bus %d for port %d\n",
			me->chip_name, r.bus_type, me->ec_pd_id);
		return -1;
	}

	if (r.i2c_info.addr_flags != PAGE_3) {
		printf("%s: Unexpected addr 0x%02x for port %d\n",
			me->chip_name, r.i2c_info.addr_flags, me->ec_pd_id);
		return -1;
	}

	me->bus = new_cros_ec_tunnel_i2c(cros_ec_get(), r.i2c_info.port);
	return ret;
}

static vb2_error_t ps8751_ec_tunnel_status(const VbootAuxfwOps *vbaux,
					   int *protected)
{
	Ps8751 *const me = container_of(vbaux, Ps8751, fw_ops);

	if (cros_ec_tunnel_i2c_protect_status(me->bus, protected) < 0) {
		printf("%s: could not get EC I2C tunnel status!\n",
		       me->chip_name);
		return VB2_ERROR_UNKNOWN;
	}
	return VB2_SUCCESS;
}

static int ps8751_ec_pd_suspend(Ps8751 *me)
{
	int status;

	status = cros_ec_pd_control(me->ec_pd_id, PD_SUSPEND);
	if (status == -EC_RES_BUSY)
		printf("%s: PD_SUSPEND busy! Could be only power source.\n",
		       me->chip_name);
	else if (status != 0)
		printf("%s: PD_SUSPEND failed (%d)!\n", me->chip_name, status);
	return status;
}

static int ps8751_ec_pd_resume(Ps8751 *me)
{
	int status;

	status = cros_ec_pd_control(me->ec_pd_id, PD_RESUME);
	if (status != 0)
		printf("%s: PD_RESUME failed!\n", me->chip_name);
	return status;
}

/**
 * dump flash content
 *
 * @param me		device context
 * @param offset_start	flash device offset to dump
 * @param size		size of data to dump
 */

static void ps8751_dump_flash(Ps8751 *me,
			      const uint32_t offset_start,
			      const uint32_t size)
{
	uint32_t offset_end  = offset_start + size;
	uint8_t prev_buf[PS_FW_RD_CHUNK];
	uint8_t buf[PS_FW_RD_CHUNK];
	uint64_t deadline = 0;
	uint32_t offset;
	int dot3 = 0;
	int i;

	printf("================================"
	       "================================\n{\n");

	for (offset = offset_start;
	     offset < offset_end;
	     offset += sizeof(buf)) {
		if (ps8751_keep_awake(me, &deadline) != 0)
			break;

		/* construct a SPI read PS_FW_CHUNK bytes @ offset command */
		if (ps8751_spi_setup_cmd24(me,
					   SPI_CMD_READ_DATA, offset) != 0) {
			debug("could not set up addr\n");
			break;
		}

		static const I2cWriteVec trig[] = {
			{ P2_SPI_LEN,
			  ((PS_FW_RD_CHUNK - 1) << 4) | (4 - 1) },
			{ P2_SPI_CTRL, P2_SPI_CTRL_TRIGGER },
		};
		if (write_regs(me, PAGE_2, trig, ARRAY_SIZE(trig)) != 0) {
			debug("could not issue SPI_CTRL\n");
			break;
		}
		if (ps8751_spi_fifo_wait_busy(me) != 0)
			return;

		for (i = 0; i < sizeof(buf); ++i) {
			if (read_reg(me, PAGE_2, P2_RD_FIFO, &buf[i]) != 0)
				break;
		}
		if (i != sizeof(buf))
			break;

		if (offset > offset_start &&
		    memcmp(buf, prev_buf, sizeof(buf)) == 0) {
			if (!dot3) {
				printf("...\n");
				dot3 = 1;
			}
		} else {
			memcpy(prev_buf, buf, sizeof(prev_buf));
			printf("0x%06x: ", offset);
			for (i = 0; i < sizeof(buf); ++i)
				printf(" %02x", buf[i]);
			printf("\n");
			dot3 = 0;
		}
	}

	printf("\n}\n================================"
	       "================================\n");
}

/**
 * install a new firmware image, replacing whatever was there before,
 * then verify installed data.
 *
 * the MPU is assumed to be stopped (highly recommended)
 * the SPI bus and flash are write-enabled
 *
 * @param me		device context
 * @param data		addr of firmware blob to install
 * @param data_size	size of firmware blob to install
 * @return 0 if ok, -1 on error
 */

static int ps8751_reflash(Ps8751 *me, const uint8_t *data, size_t data_size)
{
	int status;

	debug("data %8p len %zu\n", data, data_size);

	if (PS8751_DEBUG >= 2)
		ps8751_dump_flash(me, PARADE_FW_START,
				  PARADE_FW_END - PARADE_FW_START);

	status = ps8751_erase(me, PARADE_FW_START, data_size);
	if (status != 0) {
		printf("%s: FW erase failed\n", me->chip_name);
		return -1;
	}
	/*
	 * quick confidence check to see if we modified flash
	 * we'll do a full verify after programming
	 */
	status = ps8751_verify(me, PARADE_FW_START,
			       erased_bytes,
			       MIN(data_size, sizeof(erased_bytes)));
	if (status != 0) {
		printf("%s: FW erase verify failed\n", me->chip_name);
		return -1;
	}

	if (PS8751_DEBUG >= 2) {
		debug("start post erase 7s delay...\n");
		mdelay(7 * 1000);
		debug("end post erase delay\n");
	}

	if (PS8751_DEBUG >= 2)
		ps8751_dump_flash(me, PARADE_FW_START,
				  PARADE_FW_END - PARADE_FW_START);
	status = ps8751_program(me, PARADE_FW_START, data, data_size);
	if (PS8751_DEBUG >= 2)
		ps8751_dump_flash(me, PARADE_FW_START, PARADE_TEST_FW_SIZE);
	if (status != 0) {
		printf("%s: FW program failed\n", me->chip_name);
		return -1;
	}

	if (ps8751_verify(me, PARADE_FW_START, data, data_size) != 0) {
		if (PS8751_DEBUG > 0)
			ps8751_dump_flash(me, PARADE_FW_START, data_size);
		return -1;
	}

	return 0;
}

/*
 * reading the firmware takes about 15-20 secs, so we'll just use the
 * firmware rev as a trivial hash.
 */

static vb2_error_t ps8751_check_hash(const VbootAuxfwOps *vbaux,
				     const uint8_t *hash, size_t hash_size,
				     enum vb2_auxfw_update_severity *severity)
{
	Ps8751 *me = container_of(vbaux, Ps8751, fw_ops);
	enum ps8751_device_state status;

	debug("call...\n");

	if (hash_size != 2) {
		debug("hash_size %zu unexpected\n", hash_size);
		return VB2_ERROR_INVALID_PARAMETER;
	}

	status = ps8751_capture_device_id(me, 0);
	if (status == PS8751_DEVICE_MISSING) {
		*severity = VB2_AUXFW_NO_DEVICE;
		printf("Skipping upgrade for %s\n", me->chip_name);
		return VB2_SUCCESS;
	} else if (status == PS8751_DEVICE_NOT_PARADE) {
		*severity = VB2_AUXFW_NO_UPDATE;
		printf("No update required for %s\n", me->chip_name);
		return VB2_SUCCESS;
	}

	me->blob_hw_version = hash[0];
	if (hash[1] == me->chip.fw_rev) {
		*severity = VB2_AUXFW_NO_UPDATE;
		return VB2_SUCCESS;
	}
	*severity = VB2_AUXFW_SLOW_UPDATE;

	switch (ps8751_ec_pd_suspend(me)) {
	case -EC_RES_BUSY:
		*severity = VB2_AUXFW_NO_UPDATE;
		break;
	case EC_RES_SUCCESS:
		break;
	default:
		*severity = VB2_AUXFW_NO_DEVICE;
		return VB2_ERROR_UNKNOWN;
	}

	debug("update severity %d\n", *severity);
	return VB2_SUCCESS;
}

static int ps8751_halt_and_flash(Ps8751 *me,
				 const uint8_t *image, size_t image_size)
{
	int status = -1;

	if (ps8751_rom_ctrl(me) != 0)
		return -1;
	if (ps8751_disable_mpu(me) != 0)
		return -1;
	if (ps8751_spi_flash_unlock(me) != 0)
		goto enable_mpu;
	debug("unlock_spi_bus returned\n");
	if (ps8751_spi_flash_identify(me) == 0 &&
	    ps8751_reflash(me, image, image_size) == 0)
		status = 0;

	if (ps8751_spi_flash_lock(me) != 0)
		status = -1;

 enable_mpu:
	if (ps8751_enable_mpu(me) != 0)
		return -1;
	return status;
}

static vb2_error_t ps8751_update_image(const VbootAuxfwOps *vbaux,
				       const uint8_t *image, size_t image_size)
{
	Ps8751 *me = container_of(vbaux, Ps8751, fw_ops);
	vb2_error_t status = VB2_ERROR_UNKNOWN;
	int protected;
	int timeout;

	debug("call...\n");

	/* If the I2C tunnel is not known, probe EC for that */
	if (!me->bus && ps8751_construct_i2c_tunnel(me)) {
		printf("%s: Error constructing i2c tunnel\n", me->chip_name);
		return VB2_ERROR_UNKNOWN;
	}

	if (ps8751_ec_tunnel_status(vbaux, &protected) != 0)
		return VB2_ERROR_UNKNOWN;
	if (protected)
		return VB2_REQUEST_REBOOT_EC_TO_RO;

	if (image == NULL || image_size == 0)
		return VB2_ERROR_INVALID_PARAMETER;

	if (ps8751_wake_i2c(me) != 0)
		goto pd_resume;

	if (!ps8751_is_fw_compatible(me, image))
		goto hide_i2c;
	if (ps8751_halt_and_flash(me, image, image_size) == 0)
		status = VB2_SUCCESS;

 hide_i2c:
	if (ps8751_hide_i2c(me) != 0)
		status = VB2_ERROR_UNKNOWN;

 pd_resume:
	if (ps8751_ec_pd_resume(me) != 0)
		status = VB2_ERROR_UNKNOWN;

	/* Wait at most ~60ms for reset to occur. */
	timeout = PS_RESTART_DELAY_CS;
	do {
		if (ps8751_capture_device_id(me, 1) == PS8751_DEVICE_PRESENT)
			break;

		mdelay(10);
		timeout--;
	} while (timeout > 0);

	if (timeout == 0)
		status = VB2_ERROR_UNKNOWN;

	return status;
}

static const VbootAuxfwOps ps8751_fw_ops = {
	.fw_image_name = "ps8751_a3.bin",
	.fw_hash_name = "ps8751_a3.hash",
	.check_hash = ps8751_check_hash,
	.update_image = ps8751_update_image,
};

static const VbootAuxfwOps ps8751_fw_canary_ops = {
	.fw_image_name = "ps8751_a3_canary.bin",
	.fw_hash_name = "ps8751_a3_canary.hash",
	.check_hash = ps8751_check_hash,
	.update_image = ps8751_update_image,
};

static const VbootAuxfwOps ps8755_fw_ops = {
	.fw_image_name = "ps8755_a2.bin",
	.fw_hash_name = "ps8755_a2.hash",
	.check_hash = ps8751_check_hash,
	.update_image = ps8751_update_image,
};

static const VbootAuxfwOps ps8705_a2_fw_ops = {
	.fw_image_name = "ps8705_a2.bin",
	.fw_hash_name = "ps8705_a2.hash",
	.check_hash = ps8751_check_hash,
	.update_image = ps8751_update_image,
};

static const VbootAuxfwOps ps8705_a3_fw_ops = {
	.fw_image_name = "ps8705_a3.bin",
	.fw_hash_name = "ps8705_a3.hash",
	.check_hash = ps8751_check_hash,
	.update_image = ps8751_update_image,
};

static const VbootAuxfwOps ps8805_a2_fw_ops = {
	.fw_image_name = "ps8805_a2.bin",
	.fw_hash_name = "ps8805_a2.hash",
	.check_hash = ps8751_check_hash,
	.update_image = ps8751_update_image,
};

static const VbootAuxfwOps ps8805_a3_fw_ops = {
	.fw_image_name = "ps8805_a3.bin",
	.fw_hash_name = "ps8805_a3.hash",
	.check_hash = ps8751_check_hash,
	.update_image = ps8751_update_image,
};

static const VbootAuxfwOps ps8815_a0_fw_ops = {
	.fw_image_name = "ps8815_a0.bin",
	.fw_hash_name = "ps8815_a0.hash",
	.check_hash = ps8751_check_hash,
	.update_image = ps8751_update_image,
};

static const VbootAuxfwOps ps8815_a1_fw_ops = {
	.fw_image_name = "ps8815_a1.bin",
	.fw_hash_name = "ps8815_a1.hash",
	.check_hash = ps8751_check_hash,
	.update_image = ps8751_update_image,
};

static const VbootAuxfwOps ps8815_a2_fw_ops = {
	.fw_image_name = "ps8815_a2.bin",
	.fw_hash_name = "ps8815_a2.hash",
	.check_hash = ps8751_check_hash,
	.update_image = ps8751_update_image,
};

Ps8751 *new_ps8751(CrosECTunnelI2c *bus, int ec_pd_id)
{
	Ps8751 *me = xzalloc(sizeof(*me));

	me->bus = bus;
	me->ec_pd_id = ec_pd_id;
	me->fw_ops = ps8751_fw_ops;
	me->chip_type = CHIP_PS8751;
	snprintf(me->chip_name, sizeof(me->chip_name), "ps8751.%d", ec_pd_id);

	return me;
}

Ps8751 *new_ps8751_canary(CrosECTunnelI2c *bus, int ec_pd_id)
{
	Ps8751 *me = xzalloc(sizeof(*me));

	me->bus = bus;
	me->ec_pd_id = ec_pd_id;
	me->fw_ops = ps8751_fw_canary_ops;
	me->chip_type = CHIP_PS8751;
	snprintf(me->chip_name, sizeof(me->chip_name), "ps8751.%d", ec_pd_id);

	return me;
}

Ps8751 *new_ps8755(CrosECTunnelI2c *bus, int ec_pd_id)
{
	Ps8751 *me = xzalloc(sizeof(*me));

	me->bus = bus;
	me->ec_pd_id = ec_pd_id;
	me->fw_ops = ps8755_fw_ops;
	me->chip_type = CHIP_PS8755;
	snprintf(me->chip_name, sizeof(me->chip_name), "ps8755.%d", ec_pd_id);

	return me;
}

Ps8751 *new_ps8705(CrosECTunnelI2c *bus, int ec_pd_id,
		   const VbootAuxfwOps *fw_ops)
{
	Ps8751 *me = xzalloc(sizeof(*me));

	me->bus = bus;
	me->ec_pd_id = ec_pd_id;
	me->fw_ops = *fw_ops;
	me->chip_type = CHIP_PS8705;
	snprintf(me->chip_name, sizeof(me->chip_name), "ps8705.%d", ec_pd_id);

	return me;
}

Ps8751 *new_ps8705_a2(CrosECTunnelI2c *bus, int ec_pd_id)
{
	return new_ps8705(bus, ec_pd_id, &ps8705_a2_fw_ops);
}

Ps8751 *new_ps8705_a3(CrosECTunnelI2c *bus, int ec_pd_id)
{
	return new_ps8705(bus, ec_pd_id, &ps8705_a3_fw_ops);
}

static Ps8751 *new_ps8805(CrosECTunnelI2c *bus, int ec_pd_id,
			  const VbootAuxfwOps *fw_ops)
{
	Ps8751 *me = xzalloc(sizeof(*me));

	me->bus = bus;
	me->ec_pd_id = ec_pd_id;
	me->fw_ops = *fw_ops;
	me->chip_type = CHIP_PS8805;
	snprintf(me->chip_name, sizeof(me->chip_name), "ps8805.%d", ec_pd_id);

	return me;
}

Ps8751 *new_ps8805_a2(CrosECTunnelI2c *bus, int ec_pd_id)
{
	return new_ps8805(bus, ec_pd_id, &ps8805_a2_fw_ops);
}

Ps8751 *new_ps8805_a3(CrosECTunnelI2c *bus, int ec_pd_id)
{
	return new_ps8805(bus, ec_pd_id, &ps8805_a3_fw_ops);
}

static Ps8751 *new_ps8815(CrosECTunnelI2c *bus, int ec_pd_id,
			  const VbootAuxfwOps *fw_ops)
{
	Ps8751 *me = xzalloc(sizeof(*me));

	me->bus = bus;
	me->ec_pd_id = ec_pd_id;
	me->fw_ops = *fw_ops;
	me->chip_type = CHIP_PS8815;
	snprintf(me->chip_name, sizeof(me->chip_name), "ps8815.%d", ec_pd_id);

	return me;
}

Ps8751 *new_ps8815_a0(CrosECTunnelI2c *bus, int ec_pd_id)
{
	return new_ps8815(bus, ec_pd_id, &ps8815_a0_fw_ops);
}

Ps8751 *new_ps8815_a1(CrosECTunnelI2c *bus, int ec_pd_id)
{
	return new_ps8815(bus, ec_pd_id, &ps8815_a1_fw_ops);
}

Ps8751 *new_ps8815_a2(CrosECTunnelI2c *bus, int ec_pd_id)
{
	return new_ps8815(bus, ec_pd_id, &ps8815_a2_fw_ops);
}

static const VbootAuxfwOps *new_ps8xxx_from_chip_info(
	struct ec_response_pd_chip_info *r, uint8_t ec_pd_id)
{
	Ps8751 *ps8751;

	switch (r->product_id) {
	case PARADE_PS8751_PRODUCT_ID:
		ps8751 = new_ps8751(NULL, ec_pd_id);
		break;
	case PARADE_PS8755_PRODUCT_ID:
		ps8751 = new_ps8755(NULL, ec_pd_id);
		break;
	case PARADE_PS8705_PRODUCT_ID:
		switch (r->device_id) {
		case PARADE_PS8705_A2_DEVICE_ID:
			ps8751 = new_ps8705_a2(NULL, ec_pd_id);
			break;
		case PARADE_PS8705_DEFAULT_DEVICE_ID:
		case PARADE_PS8705_A3_DEVICE_ID:
			ps8751 = new_ps8705_a3(NULL, ec_pd_id);
			break;
		default:
			return NULL;
		}
		break;
	case PARADE_PS8805_PRODUCT_ID:
		switch (r->device_id) {
		case PARADE_PS8805_A2_DEVICE_ID:
			ps8751 = new_ps8805_a2(NULL, ec_pd_id);
			break;
		case PARADE_PS8805_A3_DEVICE_ID:
			ps8751 = new_ps8805_a3(NULL, ec_pd_id);
			break;
		default:
			return NULL;
		}
		break;
	case PARADE_PS8815_PRODUCT_ID:
		switch (r->device_id) {
		case PARADE_PS8815_A0_DEVICE_ID:
			ps8751 = new_ps8815_a0(NULL, ec_pd_id);
			break;
		case PARADE_PS8815_A1_DEVICE_ID:
			ps8751 = new_ps8815_a1(NULL, ec_pd_id);
			break;
		case PARADE_PS8815_A2_DEVICE_ID:
			ps8751 = new_ps8815_a2(NULL, ec_pd_id);
			break;
		default:
			return NULL;
		}
		break;
	default:
		return NULL;
	}
	ps8751->chip.vendor = r->vendor_id;
	ps8751->chip.product = r->product_id;
	ps8751->chip.device = r->device_id;
	ps8751->chip.fw_rev = r->fw_version_number;
	printf("%s: vendor 0x%04x product 0x%04x device 0x%04x fw_rev 0x%02x\n",
	       ps8751->chip_name, ps8751->chip.vendor, ps8751->chip.product,
	       ps8751->chip.device, ps8751->chip.fw_rev);
	return &ps8751->fw_ops;
}

static CrosEcAuxfwChipInfo aux_fw_ps8751_info = {
	.vid = PARADE_VENDOR_ID,
	.pid = PARADE_PS8751_PRODUCT_ID,
	.new_chip_aux_fw_ops = new_ps8xxx_from_chip_info,
};

static CrosEcAuxfwChipInfo aux_fw_ps8755_info = {
	.vid = PARADE_VENDOR_ID,
	.pid = PARADE_PS8755_PRODUCT_ID,
	.new_chip_aux_fw_ops = new_ps8xxx_from_chip_info,
};

static CrosEcAuxfwChipInfo aux_fw_ps8705_info = {
	.vid = PARADE_VENDOR_ID,
	.pid = PARADE_PS8705_PRODUCT_ID,
	.new_chip_aux_fw_ops = new_ps8xxx_from_chip_info,
};

static CrosEcAuxfwChipInfo aux_fw_ps8805_info = {
	.vid = PARADE_VENDOR_ID,
	.pid = PARADE_PS8805_PRODUCT_ID,
	.new_chip_aux_fw_ops = new_ps8xxx_from_chip_info,
};

static CrosEcAuxfwChipInfo aux_fw_ps8815_info = {
	.vid = PARADE_VENDOR_ID,
	.pid = PARADE_PS8815_PRODUCT_ID,
	.new_chip_aux_fw_ops = new_ps8xxx_from_chip_info,
};

static int ps8751_register(void)
{
	list_insert_after(&aux_fw_ps8751_info.list_node,
			  &ec_aux_fw_chip_list);
	list_insert_after(&aux_fw_ps8755_info.list_node,
			  &ec_aux_fw_chip_list);
	list_insert_after(&aux_fw_ps8705_info.list_node,
			  &ec_aux_fw_chip_list);
	list_insert_after(&aux_fw_ps8805_info.list_node,
			  &ec_aux_fw_chip_list);
	list_insert_after(&aux_fw_ps8815_info.list_node,
			  &ec_aux_fw_chip_list);
	return 0;
}

INIT_FUNC(ps8751_register);
