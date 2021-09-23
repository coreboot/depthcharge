/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2021 Google LLC.
 *
 * Private interfaces for the Parade PS8751 driver.
 */

#ifndef __DRIVERS_EC_PS8751_PRIV_H__
#define __DRIVERS_EC_PS8751_PRIV_H__

#include "drivers/ec/ps8751/ps8751.h"

#ifndef __must_check
#define __must_check	__attribute__((warn_unused_result))
#endif

#define PS8751_DEBUG	0

#if (PS8751_DEBUG > 0)
#define debug(msg, args...)	printf("%s: " msg, __func__, ##args)
#else
#define debug(msg, args...)
#endif

/* Chip I2C peripheral addresses */

#define PAGE_0			(0x10 >> 1)
#define PAGE_1			(0x12 >> 1)
#define PAGE_2			(0x14 >> 1)
#define PAGE_3			(0x16 >> 1)	/* primary TCPCI registers */
#define PAGE_7			(0x1e >> 1)	/* mapped flash page */

/* Chip I2C registers */

#define P0_REG_SM		0x06
#define PS8751_P0_REG_SM_CHIP_REVISION_A3	0x80

#define P0_REG_REV		0x62
#define P0_REG_REV_MASK		0xf0
#define P0_REG_REV_SHIFT	4
#define PS8805_P0_REG_REV_CHIP_REVISION_A2	0x0
#define PS8805_P0_REG_REV_CHIP_REVISION_A3	0xA

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
#define P2_FLASH_A8_A15		0x8e	/* SPI flash addr[8:15] */
#define P2_FLASH_A16_A23	0x8f	/* SPI flash addr[16:23] */
#define P2_WR_FIFO		0x90
#define P2_RD_FIFO		0x91
#define P2_SPI_LEN		0x92
#define P2_SPI_CTRL		0x93
#define P2_SPI_CTRL_NOREAD	0x04
#define P2_SPI_CTRL_FIFO_RESET	0x02
#define P2_SPI_CTRL_TRIGGER	0x01
#define P2_SPI_STATUS		0x9e
#define P2_MUX_HPD		0xd0
#define P2_CLK_CTRL		0xd6

#define P2_PROG_WIN_UNLOCK	0xda
#define P2_PROG_WIN_UNLOCK_LOCK	0x00
#define P2_PROG_WIN_UNLOCK_ACK	0x01

#define P2_FLASH_A8_A15		0x8e	/* SPI flash addr[8:15] */
#define P2_FLASH_A16_A23	0x8f	/* SPI flash addr[16:23] */

#define P2_WR_FIFO		0x90
#define P2_RD_FIFO		0x91
#define P2_SPI_LEN		0x92

#define P2_SPI_CTRL		0x93
#define P2_SPI_CTRL_NOREAD	0x04
#define P2_SPI_CTRL_TRIGGER	0x01

#define P2_PROG_WIN_UNLOCK	0xda
#define P2_PROG_WIN_UNLOCK_LOCK	0x00
#define P2_PROG_WIN_UNLOCK_ACK	0x01


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

/* Integrated SPI flash parameters */

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

#define SPI_FLASH_PAGE_SIZE	(1 << 8)	/* 256B, always 2^n */
#define SPI_FLASH_PAGE_MASK	(SPI_FLASH_PAGE_SIZE - 1)

#define SPI_STATUS_WIP		0x01
#define SPI_STATUS_WEL		0x02
#define SPI_STATUS_BP		0x0c
#define SPI_STATUS_SRP		0x80

/*
 * bytes of SPI FIFO depth after command overhead
 */

#define PS_FW_RD_CHUNK		16
#define PS_FW_WR_CHUNK		12

#define PS_FW_I2C_WINDOW_SIZE	256

#define USEC_TO_SEC(us)		((us) / 1000000)
#define USEC_TO_MSEC(us)	((us) / 1000)

#define PS_SPI_TIMEOUT_US	(1 * 1000 * 1000)	/* 1s */
#define PS_WIP_TIMEOUT_US	(1 * 1000 * 1000)	/* 1s */

#define PS_I2C_WINDOW_SPEED_KHZ	400

/**
 * read a single byte from a register of a SPI target
 *
 * @param me	device context
 * @param page	i2c device address
 * @param reg	i2c register on target device
 * @param data	pointer to return byte
 * @return 0 if ok, -1 on error
 */
int __must_check ps8751_read_reg(Ps8751 *me, uint8_t page, uint8_t reg,
				 uint8_t *data);

/**
 * issue a series of i2c reads
 *
 * @param me	device context
 * @param regs	vector of i2c regs to read read
 * @param count	number of elements in regs array
 * @param data	pointer to read bytes
 * @return 0 if ok, -1 on error
 */
int __must_check ps8751_read_regs(Ps8751 *me, uint8_t chip, const uint8_t *regs,
				  const size_t count, uint8_t *data);

/**
 * write a single byte to a register of an i2c page
 *
 * @param me	device context
 * @param page	i2c device address
 * @param reg	i2c register on target device
 * @param data	byte to write to register
 * @return 0 if ok, -1 on error
 */
int __must_check ps8751_write_reg(Ps8751 *me, uint8_t page, uint8_t reg,
				  uint8_t data);

/**
 * issue a series of i2c writes
 *
 * @param me	device context
 * @param page	i2c device address
 * @param cmds	vector of i2c reg write commands
 * @param count	number of elements in ops vector
 * @return 0 if ok, -1 on error
 */
int __must_check ps8751_write_regs(Ps8751 *me, uint8_t page,
				   const I2cWriteVec *cmds, const size_t count);

/**
 * send a SPI write-enable cmd
 *
 * WRITE_ENABLE must be issued before every
 * PROGRAM, ERASE, WRITE_STATUS_REGISTER command
 *
 * @param me	device context
 * @return 0 if ok, -1 on error
 */
int __must_check ps8751_spi_cmd_enable_writes(Ps8751 *me);

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
int __must_check ps8751_spi_fifo_wait_busy(Ps8751 *me);

/**
 * write a SPI command plus a 24 bit addr to the SPI interface FIFO
 *
 * @param me	device context
 * @cmd		SPI command to issue
 * @param a24	SPI command address bits (24)
 * @return 0 if ok, -1 on error
 */
int __must_check ps8751_spi_setup_cmd24(Ps8751 *me, uint8_t cmd, uint32_t a24);

/*
 * wait for "erase/program command finished" as seen by the ps8751
 * then, wait for the WIP (write-in-progress) bit to clear on the
 * flash part itself.
 *
 * @param me	device context
 */
int __must_check ps8751_spi_wait_rom_ready(Ps8751 *me);

void ps8751_init_flash_window_ops(Ps8751 *me);
void ps8751_init_flash_fifo_ops(Ps8751 *me);

#endif /* __DRIVERS_EC_PS8751_PRIV_H__ */
