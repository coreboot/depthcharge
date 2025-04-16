/* SPDX-License-Identifier: GPL-2.0 */

	.global boot_x86_linux_jump
	.type boot_x86_linux_jump, function
boot_x86_linux_jump:
	/*
	 * CAREFUL: Stack data will be invalid from here on!
	 * Store arguments in callee-saved registers
	 * Kernel entry is the first argument
	 * Boot params is the second argument
	 */
	movq %rdi, %rbx
	movq %rsi, %r12

	/* Disable interrupts before clearing sections */
	cli

#if CONFIG(CLEAR_DATA)
	/* Calculate .data section size */
	movq $_edata, %rdx
	subq $_data, %rdx

	/* Clear .data section */
	movq $0, %rsi
	movq $_data, %rdi
	call memset

	/* Calculate .bss section size */
	movq $_ebss, %rdx
	subq $_bss, %rdx

	/* Clear .bss section */
	movq $0, %rsi
	movq $_bss, %rdi
	call memset

	/* Clear .stack section */
	movq $0, %rax
	movq $_estack, %rcx
	subq $_stack, %rcx
	shr $3, %rcx
	movq $_stack, %rdi
	rep stosq
#endif /* CONFIG(CLEAR_DATA) */

	/* Set %rax to jump address */
	movq %rbx, %rax
	/* Set %rsi to kernel params */
	movq %r12, %rsi

	/*
	 * Set %rbx, %rbp, and %rdi to 0, %rsi to point to the boot_params
	 * structure, and then jump to the kernel. We assume that %cs is
	 * 0x10, 4GB flat, and read/execute, and the data segments are 0x18,
	 * 4GB flat, and read/write.
	 */
	movq $0, %rbx
	movq $0, %rbp
	movq $0, %rdi
	jmp *%rax
