/* SPDX-License-Identifier: GPL-2.0 */

	.global boot_arm64_linux_jump
	.type boot_arm64_linux_jump, function
boot_arm64_linux_jump:
	/* Entered with X0 = &fdt, X1 = entry */

	/* No need to save callee-saved registers for no return function. */
	mov	x24, x0		/* save fdt */
	mov	x25, x1		/* save entry pointer */

	/* CAREFUL: Stack data will be invalid from here on! */

#if CONFIG(CLEAR_DATA)
	/* Clear .data section */
	mov	x1, #0
	ldr	x0, =_data
	ldr	x2, =_edata
	sub	x2, x2, x0
	bl memset

	/* Clear .bss section */
	mov	x1, #0
	ldr	x0, =_bss
	ldr	x2, =_ebss
	sub	x2, x2, x0
	bl memset

	/* Clear .stack section */
	mov	x1, #0
	ldr	x0, =_stack
	ldr	x2, =_estack
	sub	x2, x2, x0
	bl memset
#endif

	/* The stack pointer is still valid, but the stack data is zero out. */
	bl	tlb_invalidate_all
	bl	mmu_disable

	/* CAREFUL: Stack pointer will be invalid from here on! */
	mov	x0, x24		/* restore fdt */
	mov	x1, #0		/* X1 = 0 */
	mov	x2, #0		/* X2 = 0 */
	mov	x3, #0		/* X3 = 0 */
	br	x25		/* jump to entry pointer */
