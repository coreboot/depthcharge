/*
 * Copyright 2013 Google Inc.
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

#ifndef __BOOT_FIT_H__
#define __BOOT_FIT_H__

#include <stdint.h>

struct DeviceTree;
typedef struct DeviceTree DeviceTree;

int fit_load(void *fit, char *cmd_line, void **kernel, uint32_t *kernel_size,
	     DeviceTree **dt);

/*
 * Set compatible string for the preferred kernel DT. |compat| must stay
 * accessible throughout depthcharge's runtime and thus not be stack-allocated!
 */
void fit_set_compat(const char *compat);

/*
 * Shorthand to set a compatible string from a printf() pattern with %d and a
 * board revision number. Caller must ensure that |pattern| contains exactly one
 * format directive and that its replacement takes up no more than 2 characters!
 */
void fit_set_compat_by_rev(const char *pattern, int rev);

#endif /* __BOOT_FIT_H__ */
