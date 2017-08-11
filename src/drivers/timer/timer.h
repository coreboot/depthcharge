/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2017 Intel Corporation.
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
#ifndef __DRIVERS_TIMER_H__
#define __DRIVERS_TIMER_H__

struct stopwatch {
	uint64_t start;
	uint64_t expires;
};

/* Add microseconds to an absolute time. */
void stopwatch_init_usecs_expire(struct stopwatch *sw, unsigned long us);
/* Add miliseconds to an absolute time. */
void stopwatch_init_msecs_expire(struct stopwatch *sw, unsigned long ms);
/*
 * Tick and check the stopwatch for expiration. Returns non-zero on exipration.
 */
int stopwatch_expired(struct stopwatch *sw);

#endif /* __DRIVERS_TIMER_H__ */
