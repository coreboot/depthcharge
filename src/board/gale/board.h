/*
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
#ifndef __BOARD_STORM_BOARD_H__
#define __BOARD_STORM_BOARD_H__

#include "base/device_tree.h"

#define QFPROM_BASE			((void *)0x00700000)
#define QFPROM_CORR_BASE		(QFPROM_BASE + 0x40000)
#define QFPROM_CORR_PTE_ROW0_LSB	(QFPROM_CORR_BASE + 0xB8)
#define QFPROM_CORR_PTE_ROW0_MSB	(QFPROM_CORR_BASE + 0xBC)

#define ADSS_AUDIO_TXB_CBCR_REG		((void *)0x770014C)
#define TCSR_WIFI0_GLB_CFG		((void *)0x01949000)
#define TCSR_WIFI1_GLB_CFG		((void *)0x01949004)
#define TCSR_PNOC_SNOC_MEMTYPE_M0_M2	((void *)0x01957004)

int set_wifi_calibration(DeviceTree *tree);

#define TCSR_BOOT_MISC_DETECT		((void *)0x0193D100)
#define TCSR_RESET_DEBUG_SW_ENTRY	((void *)0x01940000)
#define IPQ_CRASH_MAGIC			0x10
#define IPQ_CRASH_DUMP_MAX_TRIES	3
#define IPQ_CRASH_DUMP_WAIT_SEC		5

enum gale_board_id {
	BOARD_ID_GALE_PROTO = 0,
	BOARD_ID_GALE_EVT = 1,
	BOARD_ID_GALE_EVT2 = 2,
	BOARD_ID_GALE_EVT3 = 5,
};

#define TCSR_BOOT_MISC_DETECT		((void *)0x0193D100)
#define TCSR_RESET_DEBUG_SW_ENTRY	((void *)0x01940000)
#define IPQ_CRASH_MAGIC			0x10
#define IPQ_CRASH_DUMP_MAX_TRIES	3
#define IPQ_CRASH_DUMP_WAIT_SEC		5

PowerOps *new_ipq40xx_power_ops(void);

#endif
