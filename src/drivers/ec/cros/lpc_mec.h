/*
 * Chromium OS EC driver - LPC MEC interface
 *
 * Copyright 2015 Google Inc.
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __DRIVERS_EC_CROS_LPC_MEC_H__
#define __DRIVERS_EC_CROS_LPC_MEC_H__

/* For MEC, access ranges 0x800 thru 0x9ff using EMI interface instead of LPC */
#define MEC_EMI_RANGE_START EC_HOST_CMD_REGION0
#define MEC_EMI_RANGE_END   (EC_LPC_ADDR_MEMMAP + EC_MEMMAP_SIZE)

enum {
	/* 8-bit access */
	ACCESS_TYPE_BYTE = 0x0,
	/* 16-bit access */
	ACCESS_TYPE_WORD = 0x1,
	/* 32-bit access */
	ACCESS_TYPE_LONG = 0x2,
	/*
	 * 32-bit access, read or write of MEC_EMI_EC_DATA_B3 causes the
	 * EC data register to be incremented.
	 */
	ACCESS_TYPE_LONG_AUTO_INCREMENT = 0x3,
};

/* EMI registers are relative to base */
#define MEC_EMI_BASE		0x800
#define MEC_EMI_HOST_TO_EC	(MEC_EMI_BASE + 0)
#define MEC_EMI_EC_TO_HOST	(MEC_EMI_BASE + 1)
#define MEC_EMI_EC_ADDRESS_B0	(MEC_EMI_BASE + 2)
#define MEC_EMI_EC_ADDRESS_B1	(MEC_EMI_BASE + 3)
#define MEC_EMI_EC_DATA_B0	(MEC_EMI_BASE + 4)
#define MEC_EMI_EC_DATA_B1	(MEC_EMI_BASE + 5)
#define MEC_EMI_EC_DATA_B2	(MEC_EMI_BASE + 6)
#define MEC_EMI_EC_DATA_B3	(MEC_EMI_BASE + 7)

#endif /* __DRIVERS_EC_CROS_LPC_H__ */
