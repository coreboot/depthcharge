/*
 * Copyright 2018 Google Inc.
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

#include <libpayload.h>
#include <lzma.h>
#include <cbfs.h>
#include <cbfs_ram.h>

#include "arch/cache.h"
#include "base/cleanup_funcs.h"
#include "drivers/flash/flash.h"
#include "image/fmap.h"
#include "boot/payload.h"
#include "vboot/crossystem/crossystem.h"

/*
 * payload_load() - Load an image from the given payload
 *
 * @payload: Pointer to payload containing the image to load
 * @entryp: Returns pointer to the entry point
 * @return 0 i OK, -1 on error
 */
static int payload_load(struct cbfs_payload *payload, void **entryp)
{
	struct cbfs_payload_segment *seg = &payload->segments;
	char *base = (void *)seg;

	/* Loop until we find an entry point, then return it */
	while (1) {
		void *src = base + be32toh(seg->offset);
		void *dst = (void *)(unsigned long)be64toh(seg->load_addr);
		u32 src_len = be32toh(seg->len);
		u32 dst_len = be32toh(seg->mem_len);
		int comp = be32toh(seg->compression);

		switch (seg->type) {
		case PAYLOAD_SEGMENT_CODE:
		case PAYLOAD_SEGMENT_DATA:
			printf("CODE/DATA: dst=%p dst_len=%d src=%p src_len=%d compression=%d\n",
			       dst, dst_len, src, src_len, comp);
			switch (comp) {
			case CBFS_COMPRESS_NONE:
				if (dst_len < src_len) {
					printf("Output buffer too small.\n");
					return -1;
				}
				memcpy(dst, src, src_len);
				break;
			case CBFS_COMPRESS_LZMA:
				if (!ulzman(src, src_len, dst, dst_len)) {
					printf("LZMA: Decompression failed.\n");
					return -1;
				}
				break;
			default:
				printf("Compression type %x not supported\n",
				       comp);
				return -1;
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
			*entryp = dst;
			return 0;
		default:
			printf("segment type %x not implemented. Exiting\n",
			       seg->type);
			return -1;
		}
		seg++;
	}
}

int payload_run(const char *area_name, const char *payload_name)
{
	FmapArea area;
	struct cbfs_media media;
	void *data, *entry;
	struct cbfs_payload *payload;
	int ret;

	printf("Selected '%s' from area '%s'\n", payload_name, area_name);
	if (fmap_find_area(area_name, &area)) {
		printf("Fmap region %s not found.\n", area_name);
		return 1;
	}

	data = flash_read(area.offset, area.size);
	if (!data) {
		printf("Could not read in legacy cbfs data.\n");
		return 1;
	}

	if (init_cbfs_ram_media(&media, data, area.size)) {
		printf("Could not initialize legacy cbfs.\n");
		return 1;
	}

	payload = cbfs_load_payload(&media, payload_name);
	if (!payload) {
		printf("Could not find '%s' in '%s' cbfs.\n", payload_name,
		       area_name);
		return 1;
	}

	printf("Loading %s into RAM\n", payload_name);
	ret = payload_load(payload, &entry);
	if (ret) {
		printf("Failed: error %d\n", ret);
		return 1;
	}
	crossystem_setup(FIRMWARE_TYPE_LEGACY);
	run_cleanup_funcs(CleanupOnLegacy);

	printf("Starting %s at %p...", payload_name, entry);
	cache_sync_instructions();
	selfboot(entry);

	printf("%s returned, unfortunately", payload_name);

	return 1;
}
