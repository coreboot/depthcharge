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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <string.h>
#include <endian.h>
#include <libpayload.h>
#include <lzma.h>
#include <vboot_api.h>
#include <cbfs.h>
#include <cbfs_ram.h>

#include "base/cleanup_funcs.h"
#include "drivers/flash/flash.h"
#include "image/fmap.h"
#include "vboot/boot.h"

static void load_payload_and_run(struct cbfs_payload *payload);

int VbExLegacy(void)
{
	FmapArea area;
	struct cbfs_media media;
	void *data;
	struct cbfs_payload *payload;
	const char *area_name = "RW_LEGACY";

	if (fmap_find_area(area_name, &area)) {
		printf("Fmap region %s not found.\n", area_name);
		return 1;
	}
	data = flash_read(area.offset, area.size);

	if (data == NULL) {
		printf("Could not read in legacy cbfs data.\n");
		return 1;
	}

	if (init_cbfs_ram_media(&media, data, area.size)) {
		printf("Could not initialize legacy cbfs.\n");
		return 1;
	}

	payload = cbfs_load_payload(&media, "payload");

	if (payload == NULL) {
		printf("Could not find payload in legacy cbfs.\n");
		return 1;
	}

	load_payload_and_run(payload);

	/* Should never return unless there is an error. */
	return 1;
}

static void load_payload_and_run(struct cbfs_payload *payload)
{
	/* This is a minimalistic SELF parser.  */
	struct cbfs_payload_segment *seg = &payload->segments;
	char *base = (void *)seg;

	while (1) {
		void (*payload_entry)(void);
		void *src = base + be32toh(seg->offset);
		void *dst = (void *)(unsigned long)be64toh(seg->load_addr);
		u32 src_len = be32toh(seg->len);
		u32 dst_len = be32toh(seg->mem_len);

		switch (seg->type) {
		case PAYLOAD_SEGMENT_CODE:
		case PAYLOAD_SEGMENT_DATA:
			printf("CODE/DATA: dst=%p dst_len=%d src=%p "
				"src_len=%d compression=%d\n", dst, dst_len,
				src, src_len, be32toh(seg->compression));
			if (be32toh(seg->compression) ==
						CBFS_COMPRESS_NONE) {
				if (dst_len < src_len) {
					printf("Output buffer too small.\n");
					return;
				}
				memcpy(dst, src, src_len);
			} else if (be32toh(seg->compression) ==
						CBFS_COMPRESS_LZMA) {
				unsigned long ret;
				ret = ulzman(src, src_len, dst, dst_len);
				if (ret != dst_len) {
					printf("LZMA: Decompression failed. "
						"ret=%ld, expected %d.\n", ret,
						dst_len);
					return;
				}
			} else {
				printf("Compression type %x not supported\n",
					be32toh(seg->compression));
				return;
			}
			break;
		case PAYLOAD_SEGMENT_BSS:
			printf("BSS: dst=%p len=%d\n", dst, dst_len);
			memset(dst, 0, dst_len);
			break;
		case PAYLOAD_SEGMENT_PARAMS:
			printf("PARAMS: skipped\n");
			break;
		case PAYLOAD_SEGMENT_ENTRY:
			run_cleanup_funcs(CleanupOnLegacy);
			payload_entry = dst;
			payload_entry();
			return;
		default:
			printf("segment type %x not implemented. Exiting\n",
				seg->type);
			return;
		}
		seg++;
	}
}
