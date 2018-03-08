/*
 * Copyright 2018 Google Inc.
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

#ifndef __BOARD_SMAUG_SMC_H__
#define __BOARD_SMAUG_SMC_H__

uint32_t smc_call(uint32_t arg0, uintptr_t arg1, uintptr_t arg2);

#define SMC_TOS_REGISTER_NS_DRAM_RANGES 0x72000004
#define SMC_TOS_RESUME_FID 0x72000100

#define SMC_ERR_PREEMPT_BY_IRQ -3

#endif
