// SPDX-License-Identifier: GPL-2.0

#include <commonlib/device_tree.h>
#include <commonlib/helpers.h>
#include <endian.h>
#include <libpayload.h>
#include <lz4.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "boot/android_dtboimg.h"
#include "boot/android_dt_table.h"
#include "boot/dt_update.h"
#include "image/symbols.h"

struct dt_table_entry {
	uint32_t dt_size;
	uint32_t dt_offset;
	uint32_t id;
	uint16_t min_rev;
	uint16_t max_rev;
	enum dt_compression_info compression_type;
	uint8_t id_type;
	uint32_t custom1;
	uint32_t custom2;
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

	if (ntohl(hdr->magic) != DT_TABLE_MAGIC || ntohl(hdr->version) != 1) {
		printf("%s: Invalid DTBO magic or version\n", __func__);
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

static int get_dt_table_entry(const void *buf, struct dt_table_entry *entry,
			      size_t img_size)
{
	const struct dt_table_entry_v1 *v1_entry = buf;

	entry->dt_size = ntohl(v1_entry->dt_size);
	entry->dt_offset = ntohl(v1_entry->dt_offset);
	entry->id = ntohl(v1_entry->id);
	entry->min_rev = ntohl(v1_entry->rev) & 0xffff;
	entry->max_rev = (ntohl(v1_entry->rev) >> 16) & 0xffff;
	entry->compression_type = ntohl(v1_entry->flags) & DT_COMPRESSION_MASK;
	entry->id_type = ntohl(v1_entry->custom[0]) & 0xff;
	entry->custom1 = ntohl(v1_entry->custom[1]);
	entry->custom2 = ntohl(v1_entry->custom[2]);
	if (entry->dt_offset >= img_size || (entry->dt_offset + entry->dt_size) > img_size) {
		printf("%s: ERROR: Entry @offset: %d, size: %d >= DTBO size %zu\n",
		       __func__, entry->dt_offset, entry->dt_size, img_size);
		return -1;
	}

	printf("DT Entry Size: %d, Offset: %#x, Id: %#x, Min Rev: %#x, Max Rev: %#x,"
	       " Compression Type: %#x, Id Type: %#x, custom[1..2] = [%#x, %#x]\n",
	       entry->dt_size, entry->dt_offset, entry->id, entry->min_rev, entry->max_rev,
	       entry->compression_type, entry->id_type, entry->custom1, entry->custom2);
	return 0;
}

static bool is_dt_entry_type0_compatible(const struct dt_table_entry *entry)
{
	/* 0xffffffff means matching any SKU, device will never have SKU ID 0xffffffff */
	if (entry->custom1 != 0xffffffff && entry->custom1 != lib_sysinfo.sku_id)
		return false;

	if (lib_sysinfo.board_id < entry->min_rev || lib_sysinfo.board_id > entry->max_rev)
		return false;

	return true;
}

static bool is_dt_entry_compatible(const struct dt_table_entry *entry)
{
	switch (entry->id_type) {
	case 0:
		return is_dt_entry_type0_compatible(entry);
	default:
		printf("Unsupported matching identifier version: %d\n", entry->id_type);
	}
	return false;
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

static struct list_node *stash_overlay_dt(struct device_tree *tree, uint32_t entry_id,
					  struct list_node *overlay_dt_list)
{
	struct overlay_dt *overlay = xzalloc(sizeof(*overlay));

	overlay->tree = tree;
	overlay->entry_id = entry_id;
	list_insert_after(&overlay->node, overlay_dt_list);
	return &overlay->node;
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
	struct list_node overlay_dt_list = { .next = NULL, .prev = NULL };
	struct list_node *odt_list_tail = &overlay_dt_list;

	if (!is_dtbo_image_valid(dtbo, dtbo_size))
		return NULL;

	printf("%s: Num of DTBO entries: %d\n", __func__, ntohl(hdr->dt_entry_count));
	for (uint32_t i = 0, offset = ntohl(hdr->dt_entries_offset);
		      i < ntohl(hdr->dt_entry_count);
		      i++, offset += ntohl(hdr->dt_entry_size)) {
		if (get_dt_table_entry(dtbo + offset, &entry, dtbo_size))
			continue;

		if (!is_dt_entry_compatible(&entry))
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
			odt_list_tail = stash_overlay_dt(tree, i, odt_list_tail);
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
