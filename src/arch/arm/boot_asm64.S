/*
 * Copyright 2014 Google Inc.
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

	.global boot_arm_linux_jump
	.type boot_arm_linux_jump, function
boot_arm_linux_jump:
	/* Entered with X0 = entry, X1 = machine_type, X2 = &fdt */

	/* Linux ABI expects masking of Debug, SError, IRQ and FIQ */
	mov	x3, #0x3c0
	msr	daif, x3

	mov	x4, x0	/* save entry pointer */
	mov	x0, x2	/* X0 = physical address of dtb */
	mov	x1, #0	/* X1 = 0 */
	mov	x2, #0	/* X2 = 0 */
	mov	x3, #0	/* X3 = 0 */
	br	x4		/* jump to entry pointer */
