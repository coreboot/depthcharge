/*
 * Copyright 2015 MediaTek Inc.
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

#include <libpayload.h>

#include "base/container_of.h"
#include "drivers/power/mt6397.h"

/* debug */
#define power_error(format ...)	printf("mt6397: ERROR: " format)

typedef struct {
	u32	mode;
	u32	length;
	u32	restart;
	u32	status;
	u32	interval;
	u32	swrst;
	u32	swsysrst;
} MtkWdtReg;

typedef struct {
	u32	reserved1[40];
	u32	cmd;		/* 0xA0 */
	u32	rdata;		/* 0xA4 */
	u32	vldclr;		/* 0xA8 */
} MtkPwrapWacsReg;

static inline uint32_t get_wacs0_fsm(uint32_t x)
{
	return (((x)>>16) & 0x7);
}

static inline uint32_t get_wacs0_rdata(uint32_t x)
{
	return (((x)>>0) & 0xffff);
}

static int32_t pwrap_read(Mt6397Pmic *pmic, uint32_t adr, uint32_t *rdata)
{
	uint32_t wacs_cmd = 0;
	uint32_t reg_rdata = 0;
	MtkPwrapWacsReg *wacs_regs = pmic->wacs_base;

	do {
		reg_rdata = readl(&wacs_regs->rdata);
	} while (get_wacs0_fsm(reg_rdata) != WACS_FSM_IDLE);

	wacs_cmd = (0 << 31) | ((adr >> 1) << 16);
	writel(wacs_cmd, &wacs_regs->cmd);

	do {
		reg_rdata = readl(&wacs_regs->rdata);
	} while (get_wacs0_fsm(reg_rdata) != WACS_FSM_WFVLDCLR);

	*rdata = get_wacs0_rdata(reg_rdata);
	writel(1, &wacs_regs->vldclr);

	return 0;
}

static int32_t pwrap_write(Mt6397Pmic *pmic, uint32_t adr, uint32_t wdata)
{
	uint32_t wacs_cmd = 0;
	uint32_t reg_rdata = 0;
	MtkPwrapWacsReg *wacs_regs = pmic->wacs_base;

	do {
		reg_rdata = readl(&wacs_regs->rdata);
	} while (get_wacs0_fsm(reg_rdata) != WACS_FSM_IDLE);

	wacs_cmd |= (1 << 31) | ((adr >> 1) << 16) | wdata;
	writel(wacs_cmd, &wacs_regs->cmd);

	return 0;
}

static void rtc_write_trigger(Mt6397Pmic *pmic)
{
	uint32_t rdata;

	pwrap_write(pmic, (uintptr_t)RTC_WRTGR, 1);

	do {
		pwrap_read(pmic, (uintptr_t) RTC_BBPU, &rdata);
	} while (rdata & RTC_BBPU_CBUSY);
}

/*
 * It's unlock scheme to prevent RTC miswriting. The RTC write interface is
 * protected by RTC_PROT. Whether the RTC writing interface is enabled or not
 * is decided by RTC_PROT contents. Users have to perform unlock flow to enable
 * the writing interface.
 */

static void rtc_writeif_unlock(Mt6397Pmic *pmic)
{
	pwrap_write(pmic, (uintptr_t)RTC_PROT, RTC_PROT_UNLOCK_1);
	rtc_write_trigger(pmic);
	pwrap_write(pmic, (uintptr_t)RTC_PROT, RTC_PROT_UNLOCK_2);
	rtc_write_trigger(pmic);
}

static void rtc_bbpu_power_down(Mt6397Pmic *pmic)
{
	rtc_writeif_unlock(pmic);
	pwrap_write(pmic, (uintptr_t)RTC_BBPU,
		    RTC_BBPU_KEY | RTC_BBPU_AUTO | RTC_BBPU_PWREN);
	rtc_write_trigger(pmic);
}

static int mt6397_cold_reboot(PowerOps *me)
{
	Mt6397Pmic *pmic = container_of(me, Mt6397Pmic, ops);
	MtkWdtReg *wdt_regs = pmic->wdt_base;

	/* PMIC warm reboot mode and watchdog reset mode are set in coreboot.
	 * Just trigger watchdog reset here. */
	writel(MTK_WDT_SWRST_KEY, &wdt_regs->swrst);

	halt();
}

static int mt6397_power_off(PowerOps *me)
{
	Mt6397Pmic *pmic = container_of(me, Mt6397Pmic, ops);

	rtc_bbpu_power_down(pmic);

	halt();
}

Mt6397Pmic *new_mt6397_power(uintptr_t pwrap_reg_addr, uintptr_t wdt_reg_addr)
{
	Mt6397Pmic *pmic = xzalloc(sizeof(*pmic));

	pmic->ops.power_off = &mt6397_power_off;
	pmic->ops.cold_reboot = &mt6397_cold_reboot;
	pmic->wacs_base = (void *)pwrap_reg_addr;
	pmic->wdt_base = (void *)wdt_reg_addr;

	return pmic;
}
