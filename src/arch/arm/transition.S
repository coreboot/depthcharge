.section .text, "ax", %progbits
	.global switch_to_el2
	.align 4
switch_to_el2:
	mov	 x0, #0x531
	msr	 scr_el3, x0
	mrs	 x0, SCTLR_EL2
	ldr	 x1, .SCTLR_MASK
	and	 x0, x0, x1
	msr	 SCTLR_EL2, x0
	mrs	 x0, vbar_el3
	msr	 vbar_el2, x0
	mov	 x0, #0x3c8
	msr	 spsr_el3, x0
	mrs	 x0, mair_el3
	msr	 mair_el2, x0
	mrs	 x0, tcr_el3
	msr	 tcr_el2, x0
	mrs	 x0, ttbr0_el3
	msr	 ttbr0_el2, x0
	mrs	 x0, sctlr_el2
	orr	 x0, x0, #1
	orr	 x0, x0, #1 << 2
	orr	 x0, x0, #1 << 12
	msr	 sctlr_el2, x0
	msr	 elr_el3, x30
	eret
	.align 4
		 .SCTLR_MASK:
		 .quad 0x0FFFFEFF0
