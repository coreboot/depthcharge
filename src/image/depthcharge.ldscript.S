/*
 * Copyright (C) 2008 Advanced Micro Devices, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * This file is first run through a C preprocessor before being used
 * as a linker script.  Any Kconfig options may be referenced, which
 * get sourced from build/config.h.
 */

ENTRY(_entry)

SECTIONS
{
	_kernel_start = CONFIG_KERNEL_START;
	_kernel_end = CONFIG_KERNEL_START + CONFIG_KERNEL_SIZE;
	_fit_fdt_start = CONFIG_KERNEL_FIT_FDT_ADDR;
	_fit_fdt_end = CONFIG_KERNEL_FIT_FDT_ADDR + 8M;


	/* The main portion of depthcharge. */
	. = CONFIG_BASE_ADDRESS;

	. = ALIGN(16);
	_start = .;

	.text : {
		_text = .;
		*(.text._entry)
		*(.text)
		*(.text.*)
		_etext = .;
	}

	.rodata : {
		_rodata = .;
		*(.rodata)
		*(.rodata.*)
		_erodata = .;
	}

	.data : {
		_data = .;
		*(.data)
		*(.data.*)
		_edata = .;

		_init_funcs_start = .;
		KEEP(*(.init_funcs))
		_init_funcs_end = .;
		 KEEP(*(SORT(.dp_list*)));
	}

	.got : {
		_got = .;
		*(.got)
		*(.got.*)
		_egot = .;
	}

	.bss : {
		*(.bss.ttb_buffer)
		*(.bss.pml4e)
		*(.bss.main_page_table)
		*(.bss.extra_page_table)

		_bss = .;
		*(.sbss)
		*(.sbss.*)
		*(.bss)
		*(.bss.*)
		*(COMMON)
		*(.scommon)
		_ebss = .;

		/* Stack and heap */
		. = ALIGN(16);
		_heap = .;
		. += CONFIG_HEAP_SIZE;
		. = ALIGN(16);
		_eheap = .;

		_stack = .;
		. += CONFIG_STACK_SIZE;
		. = ALIGN(16);
		_estack = .;
	}

	_end = .;

	_ = ASSERT(_kernel_end <= _start,
		   "Kernel buffer must be placed before depthcharge image!");
	_ = ASSERT(!CONFIG_KERNEL_FIT_FDT_ADDR ||
		   (_fit_fdt_end <= _start &&
		    (_kernel_end <= _fit_fdt_start ||
		     _fit_fdt_end <= _kernel_start)),
		   "FDT buffer (8MB) must be placed before depthcharge image and not overlap kernel buffer!");

	/* Stuff we don't want in the final image. */
	/DISCARD/ : {
		*(.comment)
		*(.note*)
		*(.GCC.*)
	}
}
