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
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <libpayload.h>

#include "arch/arm/boot.h"

void boot_arm_linux_jump(void *entry, uint32_t machine_type, void *fdt)
	__attribute__((noreturn));

int boot_arm_linux(uint32_t machine_type, void *fdt, void *entry)
{
	/*
	 * Requirements for entering the kernel:
	 * 1. Interrupts disabled.
	 * 2. CPU in SVC mode.
	 * 3. MMU off.
	 * 4. Data cache off.
	 */

	printf("XXX Need to clean up before jumping to the kernel. XXX\n");

	boot_arm_linux_jump(entry, machine_type, fdt);

	return 0;
}
