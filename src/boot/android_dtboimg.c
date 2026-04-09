// SPDX-License-Identifier: GPL-2.0

#include <commonlib/device_tree.h>
#include <commonlib/helpers.h>
#include <endian.h>
#include <inttypes.h>
#include <libpayload.h>
#include <lz4.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sysinfo.h>

#include "base/fw_config.h"
#include "boot/android_dtboimg.h"
#include "boot/android_dt_table.h"
#include "boot/dt_update.h"
#include "image/symbols.h"

enum dt_match_flags {
	MATCH_REV	= BIT(0),
	MATCH_SKU	= BIT(1),
	MATCH_FW_CONFIG	= BIT(2),
};

struct dt_match {
	uint32_t flags;
	uint16_t min_rev;
	uint16_t max_rev;
	uint32_t sku_id;
	uint64_t fw_config_mask;
	uint64_t fw_config_value;
};

struct dt_table_entry {
	uint32_t dt_size;
	uint32_t dt_offset;
	uint32_t id;
	enum dt_compression_info compression_type;
	struct dt_match match;
};

static bool is_dtbo_image_valid(const void *dtbo, size_t dtbo_size)
{
	struct dt_table_header *hdr = (struct dt_table_header *)dtbo;

	if (!dtbo) {
		printf("%s: DTBO image is NULL\n", __func__);
		return false;
	}

	if (dtbo_size < sizeof(*hdr)) {
		printf("%s: Invalid DTBO size (%zx)\n", __func__, dtbo_size);
		return false;
	}

	uint32_t magic = ntohl(hdr->magic);
	if (magic != DT_TABLE_MAGIC) {
		printf("%s: Invalid DTBO magic: 0x%08x\n", __func__, magic);
		return false;
	}

	if ((ntohl(hdr->dt_entries_offset) +
	     ntohl(hdr->dt_entry_count) * ntohl(hdr->dt_entry_size)) >= dtbo_size) {
		printf("%s: DT Entries overflow image size - (%d + %d * %d) > %zu\n",
		       __func__, ntohl(hdr->dt_entries_offset), ntohl(hdr->dt_entry_count),
		       ntohl(hdr->dt_entry_size), dtbo_size);
		return false;
	}
	return true;
}

static void parse_dt_match_rev(struct dt_match *match, uint32_t rev)
{
	uint16_t min_rev = rev & 0xffff;
	uint16_t max_rev = (rev >> 16) & 0xffff;

	if (!(min_rev == 0x0 && max_rev == 0xffff)) {
		match->flags |= MATCH_REV;
		match->min_rev = min_rev;
		match->max_rev = max_rev;
	}
}

static void parse_dt_match_sku(struct dt_match *match, uint32_t sku_id)
{
	match->flags |= MATCH_SKU;
	match->sku_id = sku_id;
}

static void parse_dt_match_fw_config(struct dt_match *match, uint32_t custom1,
				     uint32_t custom2, uint32_t custom3,
				     uint32_t custom4)
{
	match->flags |= MATCH_FW_CONFIG;
	match->fw_config_mask = ((uint64_t)custom1 << 32) | custom2;
	match->fw_config_value = ((uint64_t)custom3 << 32) | custom4;
}

static int get_dt_table_entry_v1(const void *buf, struct dt_table_entry *entry,
				 uint32_t dt_entry_size)
{
	const struct dt_table_entry_v1 *v1_entry = buf;
	if (dt_entry_size != sizeof(*v1_entry)) {
		printf("%s: ERROR: dt_entry_size %u != v1 entry size %zu\n",
		       __func__, dt_entry_size, sizeof(*v1_entry));
		return -1;
	}

	entry->dt_size = ntohl(v1_entry->dt_size);
	entry->dt_offset = ntohl(v1_entry->dt_offset);
	entry->id = ntohl(v1_entry->id);
	entry->compression_type = ntohl(v1_entry->flags) & DT_COMPRESSION_MASK;

	parse_dt_match_rev(&entry->match, ntohl(v1_entry->rev));

	uint8_t id_type = ntohl(v1_entry->custom[0]) & 0xff;
	if (id_type == 0) {
		parse_dt_match_sku(&entry->match, ntohl(v1_entry->custom[1]));
	} else {
		printf("%s: ERROR: Unsupported id_type: %u\n", __func__, id_type);
		return -1;
	}

	return 0;
}

static int get_dt_table_entry_v2(const void *buf, struct dt_table_entry *entry,
				 uint32_t dt_entry_size)
{
	const struct dt_table_entry_v2 *v2_entry = buf;
	if (dt_entry_size != sizeof(*v2_entry)) {
		printf("%s: ERROR: dt_entry_size %u != v2 entry size %zu\n",
		       __func__, dt_entry_size, sizeof(*v2_entry));
		return -1;
	}

	entry->dt_size = ntohl(v2_entry->dt_size);
	entry->dt_offset = ntohl(v2_entry->dt_offset);
	entry->id = ntohl(v2_entry->id);
	entry->compression_type = ntohl(v2_entry->flags) & DT_COMPRESSION_MASK;

	parse_dt_match_rev(&entry->match, ntohl(v2_entry->rev));

	uint8_t id_type = ntohl(v2_entry->custom[0]) & 0xff;
	switch (id_type) {
	case 0:
		parse_dt_match_sku(&entry->match, ntohl(v2_entry->custom[1]));
		break;
	case 1:
		parse_dt_match_fw_config(&entry->match,
					 ntohl(v2_entry->custom[1]),
					 ntohl(v2_entry->custom[2]),
					 ntohl(v2_entry->custom[3]),
					 ntohl(v2_entry->custom[4]));
		break;
	default:
		printf("%s: ERROR: Unsupported id_type: %u\n",
		       __func__, id_type);
		return -1;
	}

	return 0;
}

static int get_dt_table_entry(const void *buf, struct dt_table_entry *entry,
			      size_t img_size, uint32_t version,
			      uint32_t dt_entry_size)
{
	memset(entry, 0, sizeof(*entry));

	switch (version) {
	case 1:
		if (get_dt_table_entry_v1(buf, entry, dt_entry_size))
			return -1;
		break;
	case 2:
		if (get_dt_table_entry_v2(buf, entry, dt_entry_size))
			return -1;
		break;
	default:
		printf("%s: ERROR: Unsupported DTBO version: %u\n", __func__, version);
		return -1;
	}

	printf("[DT entry] size: %u, offset: %#x, id: %#x, compression: %#x,"
	       " match_flags: %#x, rev: [%#x..%#x], sku_id: %#x,"
	       " fw_config_mask = %#" PRIx64 ", fw_config_value = %#" PRIx64 "\n",
	       entry->dt_size, entry->dt_offset, entry->id, entry->compression_type,
	       entry->match.flags, entry->match.min_rev, entry->match.max_rev,
	       entry->match.sku_id, entry->match.fw_config_mask, entry->match.fw_config_value);

	if (entry->dt_offset >= img_size || entry->dt_size > img_size - entry->dt_offset) {
		printf("%s: ERROR: Entry @offset: %d, size: %d >= DTBO size %zu\n",
		       __func__, entry->dt_offset, entry->dt_size, img_size);
		return -1;
	}

	return 0;
}

/* 0xffffffff means matching any SKU. */
#define SKU_ID_ANY (~((uint32_t)0))

static bool is_dt_compatible(const struct dt_match *match)
{
	if (match->flags & MATCH_REV) {
		if (lib_sysinfo.board_id < match->min_rev ||
		    lib_sysinfo.board_id > match->max_rev)
			return false;
	}

	if (match->flags & MATCH_SKU) {
		if (match->sku_id != SKU_ID_ANY &&
		    match->sku_id != lib_sysinfo.sku_id)
			return false;
	}

	if (match->flags & MATCH_FW_CONFIG) {
		bool match_any = (match->fw_config_mask == 0x0 &&
				  match->fw_config_value == 0x0);
		bool match_unprovisioned = (match->fw_config_mask == ~0ULL &&
					    match->fw_config_value == UNDEFINED_FW_CONFIG);
		bool value_matched = ((lib_sysinfo.fw_config & match->fw_config_mask) ==
				      match->fw_config_value);
		bool matched;

		if (match_any)
			matched = true; /* Unprovisioned device is also a match. */
		else if (match_unprovisioned)
			matched = !fw_config_is_provisioned();
		else
			matched = fw_config_is_provisioned() && value_matched;

		if (!matched)
			return false;
	}

	return true;
}

static const void *get_dt_entry_data(const void *dtbo, struct dt_table_entry *entry)
{
	/* Reuse the output FDT buffer as a convenient, guaranteed FDT-sized
	   scratchpad that is still unused (for it's real purpose) right now. */
	void *buffer = (void *)&_fit_fdt_start;
	size_t size = _fit_fdt_end - _fit_fdt_start;

	switch (entry->compression_type) {
	case NO_COMPRESSION:
		return dtbo + entry->dt_offset;
	case LZ4_COMPRESSION:
		printf("LZ4 decompressing DTBO entry @ offset %#x\n", entry->dt_offset);
		size = ulz4fn(dtbo + entry->dt_offset, entry->dt_size, buffer, size);
		break;
	default:
		printf("ERROR: Unsupported compression format(%d) for DTBO entry @ offset %x\n",
		       entry->compression_type, entry->dt_offset);
		return NULL;
	}

	void *ret_buffer = xmalloc(size);
	memcpy(ret_buffer, buffer, size);
	return ret_buffer;
}

struct overlay_dt {
	struct device_tree *tree;
	uint32_t entry_id;
	struct list_node node;
};

#define MAX_DTBO_IDX_BC_LEN 256
static char dtbo_idx_bc_buf[MAX_DTBO_IDX_BC_LEN];
static size_t dtbo_idx_bc_len = 0;

static void stash_overlay_dt(struct device_tree *tree, uint32_t entry_id,
			     struct list_node *overlay_dt_list)
{
	struct overlay_dt *overlay = xzalloc(sizeof(*overlay));

	overlay->tree = tree;
	overlay->entry_id = entry_id;
	list_append(&overlay->node, overlay_dt_list);
}

static int apply_stashed_overlay_dt(struct device_tree *base_dt,
				     struct list_node *overlay_dt_list)
{
	struct overlay_dt *overlay;
	int ret;

	list_for_each(overlay, *overlay_dt_list, node) {
		if (dt_apply_overlay(base_dt, overlay->tree) != 0) {
			printf("Failed to apply DTB overlay entry: %d\n", overlay->entry_id);
			return -1;
		}

		ret = snprintf(&dtbo_idx_bc_buf[dtbo_idx_bc_len],
			       sizeof(dtbo_idx_bc_buf) - dtbo_idx_bc_len,
			       ",%d", overlay->entry_id);
		if (ret < 0 || ret >= (sizeof(dtbo_idx_bc_buf) - dtbo_idx_bc_len)) {
			printf("Failed(%d) to populate overlay_dtb index\n", ret);
			return -1;
		}
		dtbo_idx_bc_len += ret;

		free(overlay);
	}
	return 0;
}

struct device_tree *android_parse_dtbs(const void *dtbo, size_t dtbo_size)
{
	struct device_tree *base_dt = NULL, *tree;
	struct dt_table_header *hdr = (struct dt_table_header *)dtbo;
	struct dt_table_entry entry;
	const void *dt_entry_data;
	struct list_node overlay_dt_list = {};
	uint32_t version;
	uint32_t dt_entry_size;

	if (!is_dtbo_image_valid(dtbo, dtbo_size))
		return NULL;

	version = ntohl(hdr->version);
	printf("%s: Num of DTBO entries: %u, version: %u\n", __func__,
	       ntohl(hdr->dt_entry_count), version);
	dt_entry_size = ntohl(hdr->dt_entry_size);
	for (uint32_t i = 0, offset = ntohl(hdr->dt_entries_offset);
		      i < ntohl(hdr->dt_entry_count);
		      i++, offset += dt_entry_size) {
		if (get_dt_table_entry(dtbo + offset, &entry, dtbo_size, version,
				       dt_entry_size))
			continue;

		if (!is_dt_compatible(&entry.match))
			continue;

		printf("%s: Entry %d is compatible with the board\n", __func__, i);
		dt_entry_data = get_dt_entry_data(dtbo, &entry);
		if (!dt_entry_data)
			continue;

		tree = fdt_unflatten(dt_entry_data);
		if (!tree) {
			printf("Failed to unflatten DT entry: %d\n", i);
			continue;
		}

		if (dt_is_overlay(tree)) {
			stash_overlay_dt(tree, i, &overlay_dt_list);
		} else if (!base_dt) {
			base_dt = tree;
			int ret = snprintf(dtbo_idx_bc_buf, sizeof(dtbo_idx_bc_buf), "%d", i);
			if (ret < 0 || ret >= sizeof(dtbo_idx_bc_buf)) {
				printf("Failed(%d) to populate base_dtb index\n", ret);
				return NULL;
			}
			dtbo_idx_bc_len = ret;
		} else {
			printf("ERROR: More than one base DT entry %d\n", i);
			return NULL;
		}
	}

	if (!base_dt) {
		printf("%s ERROR: Base DT is NULL\n", __func__);
		return NULL;
	}

	if (apply_stashed_overlay_dt(base_dt, &overlay_dt_list) < 0)
		return NULL;

	return base_dt;
}

const char *android_get_dtbo_indices(void)
{
	if (!dtbo_idx_bc_len)
		return NULL;

	return dtbo_idx_bc_buf;
}
