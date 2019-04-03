/*
 * Copyright 2013 Google Inc.
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
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <assert.h>
#include <endian.h>
#include <libpayload.h>
#include <lzma.h>
#include <lz4.h>
#include <stdint.h>
#include <tlcl.h>

#include "base/ranges.h"
#include "boot/fit.h"
#include "config.h"
#include "image/symbols.h"



static ListNode image_nodes;
static ListNode config_nodes;

static const char *fit_kernel_compat[10] = { NULL };
static int num_fit_kernel_compat = 0;

void fit_add_compat(const char *compat)
{
	assert(num_fit_kernel_compat < ARRAY_SIZE(fit_kernel_compat));
	fit_kernel_compat[num_fit_kernel_compat++] = compat;
}

static void fit_add_default_compats(void)
{
	u32 rev = lib_sysinfo.board_id;
	u32 sku = lib_sysinfo.sku_id;
	char compat[sizeof(CONFIG_BOARD) + 64];
	char base[sizeof(CONFIG_BOARD) + 32];
	char sku_string[16];
	char rev_string[16];

	static int done = 0;
	if (done)
		return;
	done = 1;

	sprintf(base, "google,%s", CONFIG_BOARD);
	sprintf(rev_string, "-rev%u", rev);
	sprintf(sku_string, "-sku%u", sku);

	char *c;
	for (c = base; *c != '\0'; c++)
		if (*c == '_')
			*c = '-';

	if (sku != UNDEFINED_STRAPPING_ID && rev != UNDEFINED_STRAPPING_ID) {
		sprintf(compat, "%s%s%s", base, rev_string, sku_string);
		fit_add_compat(strdup(compat));
	}

	if (sku != UNDEFINED_STRAPPING_ID) {
		sprintf(compat, "%s%s", base, sku_string);
		fit_add_compat(strdup(compat));
	}

	if (rev != UNDEFINED_STRAPPING_ID) {
		sprintf(compat, "%s%s", base, rev_string);
		fit_add_compat(strdup(compat));
	}

	fit_add_compat(strdup(base));
}

static FitImageNode *find_image(const char *name)
{
	FitImageNode *image;
	list_for_each(image, image_nodes, list_node) {
		if (!strcmp(image->name, name))
			return image;
	}
	return NULL;
}

static void image_node(DeviceTreeNode *node)
{
	FitImageNode *image = xzalloc(sizeof(*image));
	image->compression = CompressionNone;

	image->name = node->name;

	DeviceTreeProperty *prop;
	list_for_each(prop, node->properties, list_node) {
		if (!strcmp("data", prop->prop.name)) {
			image->data = prop->prop.data;
			image->size = prop->prop.size;
		} else if (!strcmp("compression", prop->prop.name)) {
			if (!strcmp("none", prop->prop.data))
				image->compression = CompressionNone;
			else if (!strcmp("lzma", prop->prop.data))
				image->compression = CompressionLzma;
			else if (!strcmp("lz4", prop->prop.data))
				image->compression = CompressionLz4;
			else
				image->compression = CompressionInvalid;
		}
	}

	list_insert_after(&image->list_node, &image_nodes);
}

static void config_node(DeviceTreeNode *node)
{
	FitConfigNode *config = xzalloc(sizeof(*config));
	config->name = node->name;

	DeviceTreeProperty *prop;
	list_for_each(prop, node->properties, list_node) {
		if (!strcmp("kernel", prop->prop.name))
			config->kernel = find_image(prop->prop.data);
		else if (!strcmp("fdt", prop->prop.name))
			config->fdt = find_image(prop->prop.data);
		else if (!strcmp("ramdisk", prop->prop.name))
			config->ramdisk = find_image(prop->prop.data);
		else if (!strcmp("compatible", prop->prop.name))
			config->compat = prop->prop;
	}

	list_insert_after(&config->list_node, &config_nodes);
}

static void fit_unpack(DeviceTree *tree, const char **default_config)
{
	DeviceTreeNode *child;
	DeviceTreeNode *images = dt_find_node_by_path(tree, "/images",
						      NULL, NULL, 0);
	if (images)
		list_for_each(child, images->children, list_node)
			image_node(child);

	DeviceTreeNode *configs = dt_find_node_by_path(tree, "/configurations",
						       NULL, NULL, 0);
	if (configs) {
		*default_config = dt_find_string_prop(configs, "default");
		list_for_each(child, configs->children, list_node)
			config_node(child);
	}
}

static int fdt_find_compat(void *blob, uint32_t start_offset, FdtProperty *prop)
{
	int offset = start_offset;
	int size;

	size = fdt_node_name(blob, offset, NULL);
	if (!size)
		return -1;
	offset += size;

	while ((size = fdt_next_property(blob, offset, prop))) {
		if (!strcmp("compatible", prop->name))
			return 0;

		offset += size;
	}

	prop->name = NULL;
	return -1;
}

static int fit_check_compat(FdtProperty *compat_prop, const char *compat_name)
{
	int bytes = compat_prop->size;
	const char *compat_str = compat_prop->data;

	for (int pos = 0; bytes && compat_str[0]; pos++) {
		if (!strncmp(compat_str, compat_name, bytes))
			return pos;
		int len = strlen(compat_str) + 1;
		compat_str += len;
		bytes -= len;
	}
	return -1;
}

static int fit_rank_compat(FitConfigNode *config)
{
	// If there was no "compatible" property in config node, this is a
	// legacy FIT image. Must extract compat prop from FDT itself.
	if (!config->compat.name) {
		void *fdt_blob = config->fdt->data;
		FdtHeader *fdt_header = (FdtHeader *)fdt_blob;
		uint32_t fdt_offset =
			betohl(fdt_header->structure_offset);

		if (config->fdt->compression != CompressionNone) {
			printf("ERROR: config %s has a compressed FDT without "
			       "external compatible property, skipping.\n",
			       config->name);
			return -1;
		}

		if (fdt_find_compat(fdt_blob, fdt_offset, &config->compat)) {
			printf("ERROR: Can't find compat string in FDT %s "
			       "for config %s, skipping.\n",
			       config->fdt->name, config->name);
			return -1;
		}
	}

	config->compat_pos = -1;
	config->compat_rank = -1;
	for (int i = 0; i < num_fit_kernel_compat; i++) {
		int pos = fit_check_compat(&config->compat,
					   fit_kernel_compat[i]);
		if (pos >= 0) {
			config->compat_pos = pos;
			config->compat_rank = i;
			break;
		}
	}

	return 0;
}

size_t fit_decompress(FitImageNode *node, void *buffer, size_t bufsize)
{
	switch (node->compression) {
	case CompressionNone:
		printf("Relocating %s to %p\n", node->name, buffer);
		memmove(buffer, node->data, MIN(node->size, bufsize));
		return node->size <= bufsize ? node->size : 0;
	case CompressionLzma:
		printf("LZMA decompressing %s to %p\n", node->name, buffer);
		return ulzman(node->data, node->size, buffer, bufsize);
	case CompressionLz4:
		printf("LZ4 decompressing %s to %p\n", node->name, buffer);
		return ulz4fn(node->data, node->size, buffer, bufsize);
	default:
		printf("ERROR: Illegal compression algorithm (%d) for %s!\n",
		       node->compression, node->name);
		return 0;
	}
}

static void *get_fdt_data(FitImageNode *fdt)
{
	// If the FDT isn't compressed, no need to alloc anything.
	if (fdt->compression == CompressionNone)
		return fdt->data;

	// Reuse the output FDT buffer as a convenient, guaranteed FDT-sized
	// scratchpad that is still unused (for it's real purpose) right now.
	void *buffer = (void *)&_fit_fdt_start;
	size_t size = &_fit_fdt_end - &_fit_fdt_start;

	size = fit_decompress(fdt, buffer, size);
	if (!size)
		return NULL;

	void *ret = malloc(size);
	if (ret)
		memcpy(ret, buffer, size);
	return ret;
}

static void update_chosen(DeviceTree *tree, char *cmd_line)
{
	int ret;
	uint64_t kaslr_seed;
	uint32_t size;
	const char *path[] = { "chosen", NULL };
	DeviceTreeNode *node = dt_find_node(tree->root, path, NULL, NULL, 1);

	/* Update only if non-NULL cmd line */
	if (cmd_line)
		dt_add_string_prop(node, "bootargs", cmd_line);

	ret = TlclGetRandom((uint8_t *)&kaslr_seed, sizeof(kaslr_seed), &size);
	if (ret || size < sizeof(kaslr_seed)) {
		printf("Failed to populate kaslr-seed\n");
		return;
	}

	dt_add_u64_prop(node, "kaslr-seed", kaslr_seed);
}

void fit_add_ramdisk(DeviceTree *tree, void *ramdisk_addr, size_t ramdisk_size)
{
	const char *path[] = { "chosen", NULL };
	DeviceTreeNode *node = dt_find_node(tree->root, path, NULL, NULL, 1);

	/* Warning: this assumes the ramdisk is currently located below 4GiB. */
	u32 start = (uintptr_t)ramdisk_addr;
	u32 end = start + ramdisk_size;

	dt_add_u32_prop(node, "linux,initrd-start", start);
	dt_add_u32_prop(node, "linux,initrd-end", end);
}

static void update_reserve_map(uint64_t start, uint64_t end, void *data)
{
	DeviceTree *tree = (DeviceTree *)data;

	DeviceTreeReserveMapEntry *entry = xzalloc(sizeof(*entry));
	entry->start = start;
	entry->size = end - start;

	list_insert_after(&entry->list_node, &tree->reserve_map);
}

typedef struct EntryParams
{
	unsigned addr_cells;
	unsigned size_cells;
	void *data;
} EntryParams;

static uint64_t max_range(unsigned size_cells)
{
	// Split up ranges who's sizes are too large to fit in #size-cells.
	// The largest value we can store isn't a power of two, so we'll round
	// down to make the math easier.
	return 0x1ULL << (size_cells * 32 - 1);
}

static void count_entries(u64 start, u64 end, void *pdata)
{
	EntryParams *params = (EntryParams *)pdata;
	unsigned *count = (unsigned *)params->data;
	u64 size = end - start;
	u64 max_size = max_range(params->size_cells);
	*count += ALIGN_UP(size, max_size) / max_size;
}

static void update_mem_property(u64 start, u64 end, void *pdata)
{
	EntryParams *params = (EntryParams *)pdata;
	u8 *data = (u8 *)params->data;
	u64 full_size = end - start;
	while (full_size) {
		const u64 max_size = max_range(params->size_cells);
		const u64 size = MIN(max_size, full_size);

		dt_write_int(data, start, params->addr_cells * sizeof(u32));
		data += params->addr_cells * sizeof(uint32_t);
		start += size;

		dt_write_int(data, size, params->size_cells * sizeof(u32));
		data += params->size_cells * sizeof(uint32_t);
		full_size -= size;
	}
	params->data = data;
}

static void update_memory(DeviceTree *tree)
{
	Ranges mem;
	Ranges reserved;
	DeviceTreeNode *node;
	u32 addr_cells = 1, size_cells = 1;
	dt_read_cell_props(tree->root, &addr_cells, &size_cells);

	// First remove all existing device_type="memory" nodes, then add ours.
	list_for_each(node, tree->root->children, list_node) {
		const char *devtype = dt_find_string_prop(node, "device_type");
		if (devtype && !strcmp(devtype, "memory"))
			list_remove(&node->list_node);
	}
	node = xzalloc(sizeof(*node));
	node->name = "memory";
	list_insert_after(&node->list_node, &tree->root->children);
	dt_add_string_prop(node, "device_type", "memory");

	// Read memory info from coreboot (ranges are merged automatically).
	ranges_init(&mem);
	ranges_init(&reserved);

#define MEMORY_ALIGNMENT (1 << 20)
	for (int i = 0; i < lib_sysinfo.n_memranges; i++) {
		struct memrange *range = &lib_sysinfo.memrange[i];
		uint64_t start = range->base;
		uint64_t end = range->base + range->size;

		/*
		 * Kernel likes its availabe memory areas at least 1MB
		 * aligned, let's trim the regions such that unaligned padding
		 * is added to reserved memory.
		 */
		if (range->type == CB_MEM_RAM) {
			uint64_t new_start = ALIGN_UP(start, MEMORY_ALIGNMENT);
			uint64_t new_end = ALIGN_DOWN(end, MEMORY_ALIGNMENT);

			if (new_start != start)
				ranges_add(&reserved, start, new_start);

			if (new_start != new_end)
				ranges_add(&mem, new_start, new_end);

			if (new_end != end)
				ranges_add(&reserved, new_end, end);
		} else {
			ranges_add(&reserved, start, end);
		}
	}

	// CBMEM regions are both carved out and explicitly reserved.
	ranges_for_each(&reserved, &update_reserve_map, tree);

	// Count the amount of 'reg' entries we need (account for size limits).
	unsigned count = 0;
	EntryParams count_params = { addr_cells, size_cells, &count };
	ranges_for_each(&mem, &count_entries, &count_params);

	// Allocate the right amount of space and fill up the entries.
	size_t length = count * (addr_cells + size_cells) * sizeof(u32);
	void *data = xmalloc(length);
	EntryParams add_params = { addr_cells, size_cells, data };
	ranges_for_each(&mem, &update_mem_property, &add_params);
	assert(add_params.data - data == length);

	// Assemble the final property and add it to the device tree.
	dt_add_bin_prop(node, "reg", data, length);
}

FitImageNode *fit_load(void *fit, char *cmd_line, DeviceTree **dt)
{
	FitImageNode *image;
	FitConfigNode *config;

	printf("Loading FIT.\n");

	DeviceTree *tree = fdt_unflatten(fit);
	if (!tree) {
		printf("Failed to unflatten FIT image!\n");
		return NULL;
	}

	const char *default_config_name = NULL;
	FitConfigNode *default_config = NULL;
	FitConfigNode *compat_config = NULL;

	fit_unpack(tree, &default_config_name);

	// List the images we found.
	list_for_each(image, image_nodes, list_node)
		printf("Image %s has %d bytes.\n", image->name, image->size);

	fit_add_default_compats();
	printf("Compat preference:");
	for (int i = 0; i < num_fit_kernel_compat; i++)
		printf(" %s", fit_kernel_compat[i]);
	printf("\n");
	// Process and list the configs.
	list_for_each(config, config_nodes, list_node) {
		if (!config->kernel) {
			printf("ERROR: config %s has no kernel, skipping.\n",
			       config->name);
			continue;
		}
		if (!config->fdt) {
			printf("ERROR: config %s has no FDT, skipping.\n",
			       config->name);
			continue;
		}

		if (fit_rank_compat(config) != 0)
			continue;

		printf("Config %s", config->name);
		if (default_config_name &&
				!strcmp(config->name, default_config_name)) {
			printf(" (default)");
			default_config = config;
		}
		printf(", kernel %s", config->kernel->name);
		printf(", fdt %s", config->fdt->name);
		if (config->ramdisk)
			printf(", ramdisk %s", config->ramdisk->name);
		if (config->compat.name) {
			printf(", compat");
			int bytes = config->compat.size;
			const char *compat_str = config->compat.data;
			for (int pos = 0; bytes && compat_str[0]; pos++) {
				printf(" %s", compat_str);
				if (pos == config->compat_pos)
					printf(" (match)");
				int len = strlen(compat_str) + 1;
				compat_str += len;
				bytes -= len;
			}

			if (config->compat_rank >= 0 && (!compat_config ||
			    config->compat_rank < compat_config->compat_rank))
				compat_config = config;
		}
		printf("\n");
	}

	FitConfigNode *to_boot = NULL;
	if (compat_config) {
		to_boot = compat_config;
		printf("Choosing best match %s for compat %s.\n",
		       to_boot->name, fit_kernel_compat[to_boot->compat_rank]);
	} else if (default_config) {
		to_boot = default_config;
		printf("No match, choosing default %s.\n", to_boot->name);
	} else {
		printf("No compatible or default configs. Giving up.\n");
		// We're leaking memory here, but at this point we're beyond
		// saving anyway.
		return NULL;
	}

	void *fdt_data = get_fdt_data(to_boot->fdt);
	if (!fdt_data) {
		printf("ERROR: Can't decompress FDT %s!\n",
		       to_boot->fdt->name);
		return NULL;
	}

	*dt = fdt_unflatten(fdt_data);
	if (!*dt) {
		printf("Failed to unflatten the kernel's fdt.\n");
		return NULL;
	}

	update_chosen(*dt, cmd_line);

	update_memory(*dt);

	if (to_boot->ramdisk) {
		if (to_boot->ramdisk->compression != CompressionNone) {
			printf("Ramdisk compression not supported.\n");
			return NULL;
		}
		fit_add_ramdisk(*dt, to_boot->ramdisk->data,
				to_boot->ramdisk->size);
	}

	return to_boot->kernel;
}
