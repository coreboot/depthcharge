/*
 * Copyright 2012 Google Inc.
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

#include <assert.h>
#include <libpayload.h>
#include <stdint.h>

#include "base/physmem.h"
#include "image/symbols.h"

/* Large pages are 2MB. */
#define LARGE_PAGE_SIZE ((1 << 20) * 2)

/*
 * Paging data structures.
 */

struct pdpe {
	uint64_t p:1;
	uint64_t mbz_0:2;
	uint64_t pwt:1;
	uint64_t pcd:1;
	uint64_t mbz_1:4;
	uint64_t avl:3;
	uint64_t base:40;
	uint64_t mbz_2:12;
};

typedef struct pdpe pdpt_t[512];

struct pde {
	uint64_t p:1;      /* present */
	uint64_t rw:1;     /* read/write */
	uint64_t us:1;     /* user/supervisor */
	uint64_t pwt:1;    /* page-level writethrough */
	uint64_t pcd:1;    /* page-level cache disable */
	uint64_t a:1;      /* accessed */
	uint64_t d:1;      /* dirty */
	uint64_t ps:1;     /* page size */
	uint64_t g:1;      /* global page */
	uint64_t avl:3;    /* available to software */
	uint64_t pat:1;    /* page-attribute table */
	uint64_t mbz_0:8;  /* must be zero */
	uint64_t base:31;  /* base address */
};

typedef struct pde pdt_t[512];

static pdpt_t pdpt __attribute__((aligned(4096)));
static pdt_t pdts[4] __attribute__((aligned(4096)));

/*
 * Map a virtual address to a physical address and optionally invalidate any
 * old mapping.
 *
 * @param virt		The virtual address to use.
 * @param phys		The physical address to use.
 * @param invlpg	Whether to use invlpg to clear any old mappings.
 */
static void x86_phys_map_page(uintptr_t virt, uint64_t phys, int invlpg)
{
	/* Extract the two bit PDPT index and the 9 bit PDT index. */
	uintptr_t pdpt_idx = (virt >> 30) & 0x3;
	uintptr_t pdt_idx = (virt >> 21) & 0x1ff;

	/* Set up a handy pointer to the appropriate PDE. */
	struct pde *pde = &(pdts[pdpt_idx][pdt_idx]);

	memset(pde, 0, sizeof(struct pde));
	pde->p = 1;
	pde->rw = 1;
	pde->us = 1;
	pde->ps = 1;
	pde->base = phys >> 21;

	if (invlpg) {
		/* Flush any stale mapping out of the TLBs. */
		__asm__ __volatile__(
			"invlpg %0\n\t"
			:
			: "m" (*(uint8_t *)virt)
		);
	}
}

/* Identity map the lower 4GB and turn on paging with PAE. */
static void x86_phys_enter_paging(void)
{
	uint64_t page_addr;
	unsigned i;

	/* Zero out the page tables. */
	memset(pdpt, 0, sizeof(pdpt));
	memset(pdts, 0, sizeof(pdts));

	/* Set up the PDPT. */
	for (i = 0; i < ARRAY_SIZE(pdts); i++) {
		pdpt[i].p = 1;
		pdpt[i].base = ((uintptr_t)&pdts[i]) >> 12;
	}

	/* Identity map everything up to 4GB. */
	for (page_addr = 0; page_addr < (1ULL << 32);
			page_addr += LARGE_PAGE_SIZE) {
		/* There's no reason to invalidate the TLB with paging off. */
		x86_phys_map_page(page_addr, page_addr, 0);
	}

	/* Turn on paging */
	__asm__ __volatile__(
		/* Load the page table address */
		"movl	%0, %%cr3\n\t"
		/* Enable pae */
		"movl	%%cr4, %%eax\n\t"
		"orl	$0x00000020, %%eax\n\t"
		"movl	%%eax, %%cr4\n\t"
		/* Enable paging */
		"movl	%%cr0, %%eax\n\t"
		"orl	$0x80000000, %%eax\n\t"
		"movl	%%eax, %%cr0\n\t"
		:
		: "r" (pdpt)
		: "eax"
	);
}

/* Disable paging and PAE mode. */
static void x86_phys_exit_paging(void)
{
	/* Turn off paging */
	__asm__ __volatile__ (
		/* Disable paging */
		"movl	%%cr0, %%eax\n\t"
		"andl	$0x7fffffff, %%eax\n\t"
		"movl	%%eax, %%cr0\n\t"
		/* Disable pae */
		"movl	%%cr4, %%eax\n\t"
		"andl	$0xffffffdf, %%eax\n\t"
		"movl	%%eax, %%cr4\n\t"
		:
		:
		: "eax"
	);
}

void arch_phys_map(uint64_t start, uint64_t size, PhysMapFunc func, void *data)
{
	if (size <= 0)
		return;

	/* Handle memory below 4GB. */
	const uint64_t max_addr = (uint64_t) ~(uintptr_t)0;
	if (start <= max_addr) {
		uint64_t cur_size = MIN(max_addr + 1 - start, size);

		void *start_ptr = (void *)(uintptr_t)start;

		assert(((uint64_t)(uintptr_t)start) == start);
		func(start, start_ptr, cur_size, data);

		start += cur_size;
		size -= cur_size;
	}

	if (size <= 0)
		return;

	/* Use paging and PAE to handle memory above 4GB up to 64GB. */
	x86_phys_enter_paging();

	/*
	 * Use virtuall address [LARGE_PAGE_SIZE, 2*LARGE_PAGE_SIZE) for
	 * mapping.
	 *
	 * Depthcharge should be far away from the beginning of memory, so
	 * that's a good region to map on top of, and make sure the virt_page
	 * will not overlap the Depthcharge.
	 */
	const uintptr_t virt_page = LARGE_PAGE_SIZE;
	assert(virt_page + LARGE_PAGE_SIZE < (uintptr_t)&_start);

	while (size > 0) {
		uintptr_t offset = start & (LARGE_PAGE_SIZE - 1);
		uint64_t phys_page = start - offset;

		uint64_t end = MIN(phys_page + LARGE_PAGE_SIZE, start + size);
		uint64_t cur_size = end - start;

		/* Map the phys_page into the virt_page and then apply func. */
		x86_phys_map_page(virt_page, phys_page, 1);
		func(start, (void *)(virt_page + offset), cur_size, data);

		start += cur_size;
		size -= cur_size;
	}

	x86_phys_exit_paging();
}
