/*
 * Copyright 2014 Google Inc.
 *
 * Copyright (C) 2011
 * Corscience GmbH & Co. KG - Simon Schwarz <schwarz@corscience.de>
 *  - Added prep subcommand support
 *  - Reorganized source - modeled after powerpc version
 *
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
 *
 * Copyright (C) 2001  Erik Mouw (J.A.K.Mouw@its.tudelft.nl)
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
 */

#include <config.h>
#include <libpayload.h>
#include <endian.h>

#include "image/symbols.h"

#include "atags.h"
#include "crc32.h"
#include "legacy_image.h"

/* Check header CRC of the uImage header */
static int image_check_hcrc(const image_header_t *hdr)
{
	uint32_t hcrc;
	image_header_t header;

	/* Copy header so we can blank CRC field for re-calculation */
	memcpy(&header, hdr, sizeof(header));
	image_set_hcrc(&header, 0);

	hcrc = crc32(0, &header, sizeof(header));

	return (hcrc == image_get_hcrc(hdr));
}

/*
 * Check uImage payload CRC, usually the kernel blob wrapped in uImage
 * header
 */
static int image_check_dcrc(const image_header_t *hdr)
{
	uint32_t dcrc = crc32(0, hdr + 1, image_get_size(hdr));

	return (dcrc == image_get_dcrc(hdr));
}

/* verify that two areas do not overlap */
static int check_overlap(const void *area1_start,
			 const void *area1_end,
			 const void *area2_start,
			 const void *area2_end)
{
	unsigned s1 = (unsigned) area1_start;
	unsigned e1 = (unsigned) area1_end;
	unsigned s2 = (unsigned) area2_start;
	unsigned e2 = (unsigned) area2_end;

	return ((s1 < s2) && (e1 > s2)) || ((s1 < e2) && (e1 > e2));
}

/*
 * Process the legacy image - basically copy it into the required location if
 * necessary. Image compression is not (yet) supported.
 */
static int bootm_load_os(bootm_header_t *bootm_header_p)
{
	image_info_t* pos = &bootm_header_p->os; /* Just to cache it. */
	uint8_t comp = pos->comp;
	uint8_t *load_buf = (uint8_t *)pos->load;
	uint8_t *image_buf = (uint8_t *)pos->image_start;
	uint32_t image_len = pos->image_len;

	switch (comp) {
	case IH_COMP_NONE:
		pos->actual_size = image_len;
		if (load_buf == image_buf)
			break; /* No relocation needed. */

		if (check_overlap(load_buf, load_buf + image_len,
				  image_buf, image_buf + image_len) ||
		    check_overlap(load_buf, load_buf + image_len,
				  &_start, &_end)) {
			printf("%s:%d - Overlap trying to relocate the kernel\n",
			       __func__, __LINE__);

			return BOOTM_ERR_OVERLAP;
		}
		printf(" relocating legacy kernel to %p ... ", load_buf);
		memcpy(load_buf, image_buf, image_len);
		printf("done!\n");
		break;

	default:
		printf("Usupported compression type %d\n", comp);
		return BOOTM_ERR_UNIMPLEMENTED;
	}

	printf("OK\n");
	return 0;
}

/* Functions to generate various ATAGs */
static struct tag *setup_start_tag (struct tag *ktags)
{
	ktags->hdr.tag = ATAG_CORE;
	ktags->hdr.size = tag_size(tag_core);

	ktags->u.core.flags = 0;
	ktags->u.core.pagesize = 0;
	ktags->u.core.rootdev = 0;

	return tag_next (ktags);
}

static struct tag *setup_commandline_tag(struct tag *ktags,
					 const char *commandline)
{
	const char *p;

	if (!commandline)
		return ktags;

	/* eat leading white space */
	for (p = commandline; *p == ' '; p++)
		;

	/* skip non-existent command lines so the kernel will still
	 * use its default command line.
	 */
	if (*p == '\0')
		return ktags;

	ktags->hdr.tag = ATAG_CMDLINE;
	ktags->hdr.size =
		(sizeof (struct tag_header) + strlen (p) + 5) >> 2;

	strcpy(ktags->u.cmdline.cmdline, p);
	printf("Kernel command line: \"%s\"\n", p);
	return tag_next(ktags);
}

static struct tag *setup_memory_tags(struct tag *ktags)
{
	for (int i = 0; i < lib_sysinfo.n_memranges; i++) {
		struct memrange *range = &lib_sysinfo.memrange[i];

		if (range->type != CB_MEM_RAM)
			continue;

		ktags->hdr.tag = ATAG_MEM;
		ktags->hdr.size = tag_size(tag_mem32);
		ktags->u.mem.start = (uint32_t) range->base;
		ktags->u.mem.size = (uint32_t) range->size;
		printf("MEM tag added %#8.8x..%#8.8x\n",
		       ktags->u.mem.start,
		       ktags->u.mem.start + ktags->u.mem.size - 1);
		ktags = tag_next(ktags);
	}
	return ktags;
}

static void setup_end_tag(struct tag *ktags)
{
	ktags->hdr.tag = ATAG_NONE;
	ktags->hdr.size = 0;
}

/* Everything seems ok - jump to the kernel passing the expected parameters. */
static int jump_to_kernel(bootm_header_t *images, struct tag *start_tag)
{
	void (*kernel_entry)(int zero, int arch, unsigned params);
	unsigned r2 = (unsigned long) start_tag;
	int mach_id = get_mach_id();

	printf("Using machine ID %d\n", mach_id);

	kernel_entry = (void (*)(int, int, unsigned))images->ep;
	kernel_entry(0, mach_id, r2);

	return -1; /* should never come here */
}

/* Prepare ATAGs, flash the cache and start the kernel */
static int start_legacy_kernel(bootm_header_t *bm_hdr_p)
{
	int ret;
	struct tag *start_tag, *current_tag;

	ret = bootm_load_os(bm_hdr_p);
	if (ret) {
		printf("%s:%d failed to load os (%d)\n",
		       __func__, __LINE__, ret);
		return ret;
	}

	start_tag = (struct tag *)CONFIG_ATAG_BASE;;
	current_tag = setup_start_tag(start_tag);
	current_tag = setup_commandline_tag(current_tag, bm_hdr_p->cmdline);
	current_tag = setup_memory_tags(current_tag);
	setup_end_tag(current_tag);

	cache_sync_instructions();
	dcache_mmu_disable();
	return jump_to_kernel(bm_hdr_p, start_tag);
}

/* verify and prepare for booting of a legacy kernel */
int legacy_boot(void *kernel, const char *cmd_line_buf)
{
	const image_header_t *hdr = kernel;
	bootm_header_t bootm_header;

	memset(&bootm_header, 0, sizeof(bootm_header));

	if (image_get_magic(hdr) != IH_MAGIC)
		return 1;

	if (!image_check_hcrc(hdr)) {
		printf("Bad Header CRC\n");
		return 1;
	}

	if (!image_check_dcrc(hdr)) {
		printf("Bad Data CRC\n");
		return 1;
	}

	bootm_header.os.type = image_get_type(hdr);
	bootm_header.os.comp = image_get_comp(hdr);
	bootm_header.os.end = (uint32_t)hdr +
		image_get_size(hdr) + sizeof(*hdr);
	bootm_header.os.load = image_get_load(hdr);
	bootm_header.os.start = (uint32_t) hdr;

	bootm_header.os.image_start = (uint32_t)(hdr + 1);
	bootm_header.os.image_len = image_get_size(hdr);

	bootm_header.ep = image_get_ep(hdr);
	bootm_header.cmdline = cmd_line_buf;

	return start_legacy_kernel(&bootm_header);
}
