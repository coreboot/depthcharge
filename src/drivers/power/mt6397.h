/*
 * Copyright 2012 Google Inc.
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

#ifndef __DRIVERS_POWER_MT6397_H__
#define __DRIVERS_POWER_MT6397_H__

#include <stdint.h>

#include "drivers/power/power.h"

enum {
	RTC_BBPU_PWREN = 1U << 0,	/* BBPU = 1 when alarm occurs */
	RTC_BBPU_BBPU = 1U << 2,	/* 1: power on, 0: power down */
	RTC_BBPU_AUTO = 1U << 3,	/* BBPU = 0 when xreset_rstb goes low */
	RTC_BBPU_CLRPKY = 1U << 4,
	RTC_BBPU_RELOAD = 1U << 5,
	RTC_BBPU_CBUSY = 1U << 6,
	RTC_BBPU_KEY = 0x43 << 8
};

/* macro for WACS_FSM */
enum {
	WACS_FSM_IDLE = 0x00,
	WACS_FSM_REQ = 0x02,
	WACS_FSM_WFDLE = 0x04,
	WACS_FSM_WFVLDCLR = 0x06,
	WACS_INIT_DONE = 0x01,
	WACS_SYNC_IDLE = 0x01,
	WACS_SYNC_BUSY = 0x00,

	TIMEOUT_READ = 10000,
	TIMEOUT_WAIT_IDLE = 10000,
};

/* Errors */
enum {
	E_PWR_INVALID_ARG = 1,
	E_PWR_INVALID_RW,
	E_PWR_INVALID_ADDR,
	E_PWR_INVALID_WDAT,
	E_PWR_INVALID_OP_MANUAL,
	E_PWR_NOT_IDLE_STATE,
	E_PWR_NOT_INIT_DONE,
	E_PWR_NOT_INIT_DONE_READ,
	E_PWR_WAIT_IDLE_TIMEOUT,
	E_PWR_WAIT_IDLE_TIMEOUT_READ
};

enum {
	RTC_BASE = 0xe000,
	RTC_BBPU = RTC_BASE + 0x0,
	RTC_PROT = RTC_BASE + 0x36,
	RTC_WRTGR = RTC_BASE + 0x3c
};

enum {
	RTC_PROT_UNLOCK_1 = 0x586a,
	RTC_PROT_UNLOCK_2 = 0x9136
};

/* WDT_SWRST */
enum {
	MTK_WDT_SWRST_KEY = 0x1209
};

typedef struct Mt6397Pmic {
	PowerOps ops;
	void *wacs_base;
	void *wdt_base;
} Mt6397Pmic;

Mt6397Pmic *new_mt6397_power(uintptr_t pwrap_reg_addr, uintptr_t wdt_reg_addr);

#endif /* __DRIVERS_POWER_MT6397_H__ */
