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
	movl 4(%esp), %ebx
	movl 8(%esp), %esi

	/* Disable interrupts before clearing sections */
	cli

#if CONFIG(CLEAR_DATA)
	/* Calculate .data section size */
	movl $_edata, %ecx
	subl $_data, %ecx

	/* Clear .data section */
	push %ecx
	push $0
	push $_data
	call memset

	/* Calculate .bss section size */
	movl $_ebss, %ecx
	subl $_bss, %ecx

	/* Clear .bss section */
	push %ecx
	push $0
	push $_bss
	call memset

	/* Clear .stack section */
	movl $0, %eax
	movl $_estack, %ecx
	subl $_stack, %ecx
	shr $2, %ecx
	movl $_stack, %edi
	rep stosl
#endif /* CONFIG(CLEAR_DATA) */

	/* Set %eax to jump address */
	movl %ebx, %eax

	/*
	 * Set %ebx, %ebp, and %edi to 0, %esi to point to the boot_params
	 * structure, and then jump to the kernel. We assume that %cs is
	 * 0x10, 4GB flat, and read/execute, and the data segments are 0x18,
	 * 4GB flat, and read/write.
	 */
	movl $0, %ebx
	movl $0, %ebp
	movl $0, %edi
	jmp *%eax
