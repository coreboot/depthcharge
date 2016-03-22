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
 */

	.arm
	.global boot_arm_linux_jump
	.type boot_arm_linux_jump, function
boot_arm_linux_jump:
	/* Entered with R0 = &fdt, R1 = entry */

	/* Linux ABI expects SVC mode (0x13) with IRQ(7) and FIQ(6) disabled */
	msr	cpsr_cxf, #0xd3

	/* CAREFUL: Stack pointer (SVC banked) will be invalid from here on! */
	mov	r3, r1		/* save entry pointer */
	mov	r1, #-1		/* R1 = 0xffffffff (Invalid Machine Type) */
	mov	r2, r0		/* R2 = &fdt */
	eor	r0, r0		/* R0 = 0 (ABI-mandated for some odd reason) */
	mov	pc, r3		/* jump to entry pointer */
