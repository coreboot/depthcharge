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

#ifndef __VBOOT_UTIL_FLAG_H__
#define __VBOOT_UTIL_FLAG_H__

struct GpioOps;
typedef struct GpioOps GpioOps;

typedef enum FlagIndex {
	/* FLAG_WPSW was here */
	/* FLAG_RECSW was here */
	FLAG_LIDSW = 2,
	FLAG_PWRSW,
	/* FLAG_ECINRW was here */
	/* FLAG_OPROM was here */
	FLAG_PHYS_PRESENCE = FLAG_PWRSW + 3,

	FLAG_MAX_FLAG
} FlagIndex;

int flag_fetch(FlagIndex index);
void flag_install(FlagIndex index, GpioOps *gpio);
void flag_replace(FlagIndex index, GpioOps *gpio);

#endif /* __VBOOT_UTIL_FLAG_H__ */
