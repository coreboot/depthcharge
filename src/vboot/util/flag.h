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

#ifndef __VBOOT_UTIL_FLAG_H__
#define __VBOOT_UTIL_FLAG_H__

struct GpioOps;
typedef struct GpioOps GpioOps;

typedef enum FlagIndex {
	FLAG_WPSW = 0,
	FLAG_RECSW,
	FLAG_DEVSW,
	FLAG_LIDSW,
	FLAG_PWRSW,
	FLAG_ECINRW,
	FLAG_OPROM,

	FLAG_MAX_FLAG
} FlagIndex;

int flag_fetch(FlagIndex index);
void flag_install(FlagIndex index, GpioOps *gpio);
void flag_replace(FlagIndex index, GpioOps *gpio);

#endif /* __VBOOT_UTIL_FLAG_H__ */
