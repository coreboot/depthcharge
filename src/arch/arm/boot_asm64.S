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
	/* Entered with X0 = entry, X1 = &fdt */

	/* Kernel is booted in EL2. Therefore switch to EL2. */
	mov	x27, x0		/* Kernel entry */
	mov	x26, x1		/* FDT pointer */

	/* Non-Secure mode in EL1 and EL0, HVC enabled, and EL2 in 64-bit. */
	mov	 x0, #0x531
	msr	 scr_el3, x0

	/* Configure sctlr_el2 to have caches off, etc. */
	mrs	 x0, SCTLR_EL2
	ldr	 x1, .SCTLR_MASK
	and	 x0, x0, x1
	msr	 SCTLR_EL2, x0

	/*
	 * debug exceptions masked, async exceptions masked, IRQ masked,
	 * FIQ masked, 64 bit mode, and returning to EL2t.
	 */
	mov	 x0, #0x3c8
	msr	 spsr_el3, x0

	/* Have EL2 entry continue on the boot path. */
	adr	x0, 1f
	msr	elr_el3, x0
	eret
1:
	/* In EL2 */
	mov	x0, x26		/* X0 = physical address of dtb */
	mov	x1, #0		/* X1 = 0 */
	mov	x2, #0		/* X2 = 0 */
	mov	x3, #0		/* X3 = 0 */

	br	x27		/* jump to entry pointer */
.align 4
	 .SCTLR_MASK:
	/*
	 * Little endian, XN not enforced, Instruction caches disabled at EL2,
	 * no stack alignment checking, data and unified cache disabled,
	 * alignment checking disabled, and MMU disabled.
	 */
	 .quad 0x0fdf7eff0
