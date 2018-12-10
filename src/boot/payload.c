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

#include "arch/cache.h"
#include "base/cleanup_funcs.h"
#include "drivers/flash/cbfs.h"
#include "drivers/flash/flash.h"
#include "image/fmap.h"
#include "boot/payload.h"
#include "vboot/crossystem/crossystem.h"

/* List of available bootloaders */
static ListNode *altfw_head;

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

int payload_run(struct cbfs_media *media, const char *payload_name)
{
	struct cbfs_payload *payload;
	void *entry;
	int ret;

	payload = cbfs_load_payload(media, payload_name);
	if (!payload) {
		printf("Could not find '%s'.\n", payload_name);
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

struct ListNode *get_altfw_list(struct cbfs_media *media)
{
	char *loaders, *ptr;
	ListNode *head, *tail;
	size_t size;

	/* Load bootloader list from cbfs */
	loaders = cbfs_get_file_content(media, "altfw/list", CBFS_TYPE_RAW,
					&size);
	if (!loaders || !size) {
		printf("%s: altfw list not found\n", __func__);
		return NULL;
	}

	printf("%s: Supported altfw boot loaders:\n", __func__);
	ptr = loaders;
	head = xzalloc(sizeof (*head));
	tail = head;
	do {
		struct altfw_info *node;
		const char *seqnum;
		char *line;

		line = strsep(&ptr, "\n");
		if (!line)
			break;
		node = xzalloc(sizeof (*node));
		seqnum = strsep(&line, ";");
		if (!seqnum)
			break;
		node->seqnum = atol(seqnum);
		node->filename = strsep(&line, ";");
		node->name = strsep(&line, ";");
		node->desc = strsep(&line, ";");
		if (!node->desc)
			break;
		printf("   %d %-15s %-15s %s\n", node->seqnum, node->name,
		       node->filename, node->desc);
		list_insert_after(&node->list_node, tail);
		tail = &node->list_node;
	} while (1);

	return head;
}

struct ListNode *payload_get_altfw_list(struct cbfs_media *media)
{
	if (cbfs_media_from_fmap("RW_LEGACY", media)) {
		printf("%s: Cannot set up CBFS\n", __func__);
		return NULL;
	}

	if (!altfw_head)
		altfw_head = get_altfw_list(media);

	return altfw_head;
}
