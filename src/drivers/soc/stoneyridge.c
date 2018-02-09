/*
 * Copyright (C) 2017 Google Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <arch/io.h>
#include <arch/msr.h>
#include <libpayload.h>
#include <stdint.h>

#include "drivers/soc/stoneyridge.h"

#define CU_PTSC_MSR	0xc0010280
#define PTSC_FREQ_MHZ	100

uint64_t timer_hz(void)
{
	return 1 * MHz;
}

uint64_t timer_raw_value(void)
{
	return _rdmsr(CU_PTSC_MSR) / PTSC_FREQ_MHZ;
}
