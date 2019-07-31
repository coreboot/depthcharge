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

#include <stdbool.h>
#include <libpayload.h>
#include <lzma.h>
#include <vb2_sha.h>
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

/* Media to use for reading payloads */
static struct cbfs_media cbfs_media;
static bool cbfs_media_valid;

#define PAYLOAD_HASH_SUFFIX ".sha256"

/*
 * get_payload_hash() - Obtain the hash for a given payload.
 *    Given the name of a payload (e.g., "altfw/XXX") appends
 *    ".sha256" to the name (e.g., "altfw/XXX.sha256") and
 *    provides a buffer of length VB2_SHA256_DIGEST_SIZE that
 *    contains the file's contents.
 * @payload_name: Name of payload
 * @return pointer to buffer on success, otherwise NULL.
 *     Caller is responsible for freeing the buffer.
 */

static uint8_t *get_payload_hash(const char *payload_name)
{
	void *data;
	size_t data_size = 0;
	char *full_name;
	size_t full_name_len = strlen(payload_name) +
			       sizeof(PAYLOAD_HASH_SUFFIX);
	full_name = xzalloc(full_name_len);
	snprintf(full_name, full_name_len, "%s%s", payload_name,
		 PAYLOAD_HASH_SUFFIX);

	/* Search in AP-RW CBFS (either FW_MAIN_A or FW_MAIN_B) */
	data = cbfs_get_file_content(CBFS_DEFAULT_MEDIA, full_name,
				     CBFS_TYPE_RAW, &data_size);
	free(full_name);
	if (data == NULL) {
		printf("Could not find hash for %s in default media cbfs.\n",
		       payload_name);
		return NULL;
	}
	if (data_size != VB2_SHA256_DIGEST_SIZE) {
		printf("Size of hash for %s is not %u: %u\n",
		       payload_name, VB2_SHA256_DIGEST_SIZE,
		       (uint32_t)data_size);
		free(data);
		data = NULL;
	}

	return (uint8_t *)data;
}

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

struct cbfs_media *payload_get_media(void)
{
	if (!cbfs_media_valid) {
		int ret;

		ret = cbfs_media_from_fmap("RW_LEGACY", &cbfs_media);
		if (ret) {
			printf("%s: Cannot set up CBFS\n", __func__);
			return NULL;
		}
		cbfs_media_valid = true;
	}

	return &cbfs_media;
}

int payload_run(const char *payload_name, int verify)
{
	struct cbfs_media *media;
	struct cbfs_payload *payload;
	size_t payload_size = 0;
	void *entry;
	int ret;

	media = payload_get_media();
	if (!media)
		return 1;

	payload = cbfs_get_file_content(media, payload_name, CBFS_TYPE_SELF,
					&payload_size);
	if (!payload) {
		printf("Could not find '%s'.\n", payload_name);
		return 1;
	}

	if (verify) {
		uint8_t real_hash[VB2_SHA256_DIGEST_SIZE];
		uint8_t *expected_hash;
		vb2_error_t rv;

		/* Calculate hash of payload. */
		rv = vb2_digest_buffer((const uint8_t *)payload,
				       payload_size, VB2_HASH_SHA256,
				       real_hash, sizeof(real_hash));
		if (rv) {
			printf("SHA-256 calculation failed for "
			       "%s payload.\n", payload_name);
			free(payload);
			return 1;
		}

		/* Retrieve the expected hash of payload stored in AP-RW. */
		expected_hash = get_payload_hash(payload_name);
		if (expected_hash == NULL) {
			printf("Could not retrieve expected hash of "
			       "%s payload.\n", payload_name);
			free(payload);
			return 1;
		}

		ret = memcmp(real_hash, expected_hash, sizeof(real_hash));
		free(expected_hash);
		if (ret != 0) {
			printf("%s payload hash check failed!\n", payload_name);
			free(payload);
			return 1;
		}
		printf("%s payload hash check succeeded.\n", payload_name);
	}

	printf("Loading %s into RAM\n", payload_name);
	ret = payload_load(payload, &entry);
	free(payload);
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

static struct ListNode *get_altfw_list(struct cbfs_media *media)
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

struct ListNode *payload_get_altfw_list(void)
{
	struct cbfs_media *media;

	media = payload_get_media();
	if (!media)
		return NULL;

	if (!altfw_head)
		altfw_head = get_altfw_list(media);

	return altfw_head;
}
