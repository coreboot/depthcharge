/*
 * Copyright 2012 Google LLC
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

#ifndef __DRIVERS_POWER_POWER_H__
#define __DRIVERS_POWER_POWER_H__

typedef struct PowerOps
{
	int (*reboot)(struct PowerOps *me);
	int (*power_off)(struct PowerOps *me);
} PowerOps;

void power_set_ops(PowerOps *ops);

/* Warm reboot the machine */
__attribute__((noreturn)) void reboot(void);

/* Power off the machine */
__attribute__((noreturn)) void power_off(void);

#endif /* __DRIVERS_POWER_POWER_H__ */
