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

#include <string.h>
#include <endian.h>
#include <libpayload.h>
#include <lzma.h>
#include <vb2_api.h>
#include <vboot_api.h>
#include <cbfs.h>
#include <cbfs_ram.h>

#include "arch/cache.h"
#include "base/cleanup_funcs.h"
#include "drivers/flash/flash.h"
#include "drivers/tpm/tpm.h"
#include "image/fmap.h"
#include "vboot/crossystem/crossystem.h"

static void load_payload_and_run(struct cbfs_payload *payload);

static uint8_t *get_legacy_hash(void);

int VbExLegacy(struct vb2_context *ctx)
{
	FmapArea area;
	struct cbfs_media media;
	void *data;
	void *payload_data;
	size_t payload_size;
	struct cbfs_payload *payload;
	const char *area_name = "RW_LEGACY";
	const char *file_name = "payload";
	uint8_t real_hash[VB2_SHA256_DIGEST_SIZE];
	uint8_t *expected_hash;
	int rv;

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

	if (!(ctx->flags & VB2_CONTEXT_DEVELOPER_MODE)) {
		printf("Developer mode not enabled - check payload hash.\n");

		payload_data = cbfs_get_file_content(
			&media, file_name, CBFS_TYPE_PAYLOAD, &payload_size);
		if (data == NULL) {
			printf("Could not find payload in legacy cbfs.\n");
			return 1;
		}

		/* Calculate hash of RW_LEGACY payload. */
		rv = vb2_digest_buffer((const uint8_t *)payload_data,
				       payload_size, VB2_HASH_SHA256, real_hash,
				       sizeof(real_hash));
		free(payload_data);
		if (rv) {
			printf("SHA-256 calculation failed for "
			       "RW_LEGACY payload.\n");
			return 1;
		}

		/* Retrieve the expected hash of RW_LEGACY payload
		   stored in AP-RW. */
		expected_hash = get_legacy_hash();
		if (expected_hash == NULL) {
			printf("Could not retrieve expected hash of "
			       "RW_LEGACY payload.\n");
		}

		/* Compare the two hashes.  This will still succeed if
		   expected_hash has garbage at the end. */
		rv = memcmp(real_hash, expected_hash, sizeof(real_hash));
		free(expected_hash);
		if (rv != 0) {
			printf("RW_LEGACY payload hash check failed!\n");
			return 1;
		}
		printf("RW_LEGACY payload hash check succeeded.\n");

		/* TPM _must_ be disabled before booting Alt OS. */
		printf("Developer mode not enabled - "
		       "disable TPM before booting.\n");
		if (tpm_set_mode(TpmModeDisabled)) {
			printf("Could not disable TPM, aborting.\n");
			return 1;
		}
	}

	payload = cbfs_load_payload(&media, file_name);

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
				if (!ulzman(src, src_len, dst, dst_len)) {
					printf("LZMA: Decompression failed.\n");
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
			crossystem_setup(FIRMWARE_TYPE_LEGACY);
			run_cleanup_funcs(CleanupOnLegacy);
			cache_sync_instructions();
			selfboot(dst);
			return;
		default:
			printf("segment type %x not implemented. Exiting\n",
				seg->type);
			return;
		}
		seg++;
	}
}

static uint8_t *get_legacy_hash(void)
{
	void *data;
	const char *file_name = "legacy_payload.sha256";

	/* Search in AP-RW CBFS (either FW_MAIN_A or FW_MAIN_B) */
	data = cbfs_get_file_content(
		CBFS_DEFAULT_MEDIA, file_name, CBFS_TYPE_RAW, NULL);

	if (data == NULL) {
		printf("Could not find %s in default media cbfs.\n", file_name);
		return NULL;
	}

	return (uint8_t *)data;
}
