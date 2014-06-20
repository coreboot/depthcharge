/*
 * Copyright 2014 Google Inc.
 *
 * Copyright (C) 1997-1999 Russell King
 *
 * This file is based on the u-boot implementation.
 *
 * See file CREDITS for list of people who contributed to this project.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef __BOOT_ATAGS_H__
#define __BOOT_ATAGS_H__

/*
 * Atags are used to pass various parameters/settings to a kernel. This file
 * incorporates the minimum set necessary to start up an arm legacy kernel.
 */

/* The list ends with an ATAG_NONE node. */
#define ATAG_NONE	0x00000000

/* The list must start with an ATAG_CORE node */
#define ATAG_CORE	0x54410001

/* command line: \0 terminated string */
#define ATAG_CMDLINE	0x54410009

/* it is allowed to have multiple ATAG_MEM nodes */
#define ATAG_MEM	0x54410002

struct tag_header {
	uint32_t size;
	uint32_t tag;
};

struct tag_cmdline {
	char	cmdline[1];	/* this is the minimum size */
};

struct tag_mem32 {
	uint32_t	size;
	uint32_t	start;	/* physical start address */
};

struct tag_core {
	u32 flags;		/* bit 0 = read-only */
	u32 pagesize;
	u32 rootdev;
};

struct tag {
	struct tag_header hdr;
	union {
		struct tag_core		core;
		struct tag_cmdline	cmdline;
		struct tag_mem32	mem;
	} u;
};

#define tag_next(t)	((struct tag *)((u32 *)(t) + (t)->hdr.size))
#define tag_size(type)	((sizeof(struct tag_header) + sizeof(struct type)) >> 2)

#endif // __BOOT_ATAGS_H__
