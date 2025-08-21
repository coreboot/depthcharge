// SPDX-License-Identifier: GPL-2.0

#include <commonlib/device_tree.h>
#include <commonlib/helpers.h>
#include <endian.h>
#include <libpayload.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "boot/android_dtboimg.h"
#include "boot/android_dt_table.h"
#include "boot/dt_update.h"

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

static bool is_dtb_image_valid(const void *dtb, size_t dtb_size)
{
	struct dt_table_header *hdr = (struct dt_table_header *)dtb;

	if (dtb_size < sizeof(*hdr)) {
		printf("%s: Invalid DTB/DTBO size (%zx)\n", __func__, dtb_size);
		return false;
	}

	if (ntohl(hdr->magic) != DT_TABLE_MAGIC || ntohl(hdr->version) != 1) {
		printf("%s: Invalid DTB/DTBO magic or version\n", __func__);
		return false;
	}

	if ((ntohl(hdr->dt_entries_offset) +
	     ntohl(hdr->dt_entry_count) * ntohl(hdr->dt_entry_size)) >= dtb_size) {
		printf("%s: DT Entries overflow image size - (%d + %d * %d) > %zu\n",
		       __func__, ntohl(hdr->dt_entries_offset), ntohl(hdr->dt_entry_count),
		       ntohl(hdr->dt_entry_size), dtb_size);
		return false;
	}
	return true;
}

static int get_dt_table_entry(const void *buf, struct dt_table_entry *entry,
			      const char *img_name, size_t img_size)
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
	if (entry->dt_offset >= img_size || (entry->dt_offset + entry->dt_size) >= img_size) {
		printf("%s: ERROR: Entry @offset: %d, size: %d >= %s size %zu\n",
		       __func__, entry->dt_offset, entry->dt_size, img_name, img_size);
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

static const void *get_base_dt_entry(const void *dtb, size_t dtb_size)
{
	struct dt_table_header *hdr = (struct dt_table_header *)dtb;
	struct dt_table_entry entry;

	if (!is_dtb_image_valid(dtb, dtb_size))
		return NULL;

	printf("%s: Num of DTB entries: %d\n", __func__, ntohl(hdr->dt_entry_count));
	for (uint32_t i = 0, offset = ntohl(hdr->dt_entries_offset);
		      i < ntohl(hdr->dt_entry_count);
		      i++, offset += ntohl(hdr->dt_entry_size)) {
		if (get_dt_table_entry(dtb + offset, &entry, "DTB", dtb_size))
			continue;

		if (is_dt_entry_compatible(&entry)) {
			printf("%s: Entry %d is compatible with the board\n", __func__, i);
			return dtb + entry.dt_offset;
		}
	}
	printf("%s: ERROR: Cannot find a matching base DTB\n", __func__);
	return NULL;
}

static void apply_dt_overlay_entries(struct device_tree *dt, const void *dtbo, size_t dtbo_size)
{
	struct dt_table_header *hdr = (struct dt_table_header *)dtbo;
	struct dt_table_entry entry;
	struct device_tree *dt_overlay;
	const void *overlay;

	if (!is_dtb_image_valid(dtbo, dtbo_size))
		return;

	printf("%s: Num of DTBO entries: %d\n", __func__, ntohl(hdr->dt_entry_count));
	for (uint32_t i = 0, offset = ntohl(hdr->dt_entries_offset);
		      i < ntohl(hdr->dt_entry_count);
		      i++, offset += ntohl(hdr->dt_entry_size)) {
		if (get_dt_table_entry(dtbo + offset, &entry, "DTBO", dtbo_size))
			continue;

		if (!is_dt_entry_compatible(&entry))
			continue;

		printf("%s: Entry %d is compatible with the board\n", __func__, i);
		overlay = dtbo + entry.dt_offset;
		dt_overlay = fdt_unflatten(overlay);
		if (!dt_overlay) {
			printf("Failed to unflatten DTB overlay entry: %d\n", i);
			continue;
		}

		if (dt_apply_overlay(dt, dt_overlay) != 0)
			printf("Failed to apply DTB overlay entry: %d\n", i);
	}
}

struct device_tree *android_parse_dtbs(void *dtb_addr, size_t dtb_size,
				       void *dtbo_addr, size_t dtbo_size)
{
	const void *base_dtb;
	struct device_tree *dt;

	if (!dtb_addr) {
		printf("Base DTB image is NULL.\n");
		return NULL;
	}

	base_dtb = get_base_dt_entry(dtb_addr, dtb_size);
	if (!base_dtb) {
		printf("No matching DTB found.\n");
		return NULL;
	}

	dt = fdt_unflatten(base_dtb);
	if (!dt) {
		printf("Failed to unflatten the matching DTB.\n");
		return NULL;
	}

	if (dtbo_addr)
		apply_dt_overlay_entries(dt, dtbo_addr, dtbo_size);

	return dt;
}
