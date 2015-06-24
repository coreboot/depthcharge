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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <assert.h>
#include <endian.h>
#include <libpayload.h>
#include <stdint.h>

#include "base/device_tree.h"
#include "base/list.h"
#include "base/ranges.h"
#include "boot/fit.h"
#include "config.h"



typedef enum CompressionType
{
	CompressionInvalid,
	CompressionNone
} CompressionType;

typedef struct FitImageNode
{
	const char *name;
	void *data;
	uint32_t size;
	CompressionType compression;

	ListNode list_node;
} FitImageNode;

typedef struct FitConfigNode
{
	const char *name;
	const char *kernel;
	FitImageNode *kernel_node;
	const char *fdt;
	FitImageNode *fdt_node;
	const char *ramdisk;
	FitImageNode *ramdisk_node;
	FdtProperty compat;
	int compat_rank;

	ListNode list_node;
} FitConfigNode;

static ListNode image_nodes;
static ListNode config_nodes;

static const char *fit_kernel_compat = "uninitialized!";

void fit_set_compat(const char *compat)
{
	fit_kernel_compat = compat;
}

void fit_set_compat_by_rev(const char *pattern, int rev)
{
	char *compat = strdup(pattern);
	sprintf(compat, pattern, rev);
	fit_set_compat(compat);
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
			config->kernel = prop->prop.data;
		else if (!strcmp("fdt", prop->prop.name))
			config->fdt = prop->prop.data;
		else if (!strcmp("ramdisk", prop->prop.name))
			config->ramdisk = prop->prop.data;
	}

	list_insert_after(&config->list_node, &config_nodes);
}

static void fit_unpack(DeviceTree *tree, const char **default_config)
{
	assert(tree && tree->root);

	DeviceTreeNode *top;
	list_for_each(top, tree->root->children, list_node) {
		DeviceTreeNode *child;
		if (!strcmp("images", top->name)) {

			list_for_each(child, top->children, list_node)
				image_node(child);

		} else if (!strcmp("configurations", top->name)) {

			DeviceTreeProperty *prop;
			list_for_each(prop, top->properties, list_node) {
				if (!strcmp("default", prop->prop.name) &&
						default_config)
					*default_config = prop->prop.data;
			}

			list_for_each(child, top->children, list_node)
				config_node(child);
		}
	}
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

static void update_chosen(DeviceTree *tree, char *cmd_line)
{
	const char *path[] = { "chosen", NULL };
	DeviceTreeNode *node = dt_find_node(tree->root, path, NULL, NULL, 1);

	dt_add_string_prop(node, "bootargs", cmd_line);
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

static void update_ramdisk(DeviceTree *tree, FitImageNode *ramdisk_node)
{
	fit_add_ramdisk(tree, ramdisk_node->data, ramdisk_node->size);
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
		const u32 size = MIN(max_size, full_size);

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

int fit_load(void *fit, char *cmd_line, void **kernel, uint32_t *kernel_size,
	     DeviceTree **dt)
{
	FdtHeader *header = (FdtHeader *)fit;
	FitImageNode *image;
	FitConfigNode *config;

	printf("Loading FIT.\n");

	if (betohl(header->magic) != FdtMagic) {
		printf("Bad FIT header magic value 0x%08x.\n",
			betohl(header->magic));
		return 1;
	}

	DeviceTree *tree = fdt_unflatten(fit);

	const char *default_config_name = NULL;
	FitConfigNode *default_config = NULL;
	FitConfigNode *compat_config = NULL;

	fit_unpack(tree, &default_config_name);

	// List the images we found.
	list_for_each(image, image_nodes, list_node)
		printf("Image %s has %d bytes.\n", image->name, image->size);

	printf("Compat preference: %s\n", fit_kernel_compat);
	// Process and list the configs.
	list_for_each(config, config_nodes, list_node) {
		if (config->kernel)
			config->kernel_node = find_image(config->kernel);
		if (config->fdt)
			config->fdt_node = find_image(config->fdt);
		if (config->ramdisk)
			config->ramdisk_node = find_image(config->ramdisk);

		if (!config->kernel_node ||
				(config->fdt && !config->fdt_node)) {
			printf("Missing image, discarding config %s.\n",
				config->name);
			FitConfigNode *prev =
				container_of(config->list_node.prev,
					FitConfigNode, list_node);
			list_remove(&config->list_node);
			free(config);
			config = prev;
			continue;
		}

		if (config->fdt_node) {
			void *fdt_blob = config->fdt_node->data;
			FdtHeader *fdt_header = (FdtHeader *)fdt_blob;
			uint32_t fdt_offset =
				betohl(fdt_header->structure_offset);
			if (fdt_find_compat(fdt_blob, fdt_offset,
					    &config->compat)) {
				config->compat_rank = -1;
				config->compat.name = NULL;
			} else {
				config->compat_rank =
					fit_check_compat(&config->compat,
							 fit_kernel_compat);
			}
		}

		printf("Config %s", config->name);
		if (default_config_name &&
				!strcmp(config->name, default_config_name)) {
			printf(" (default)");
			default_config = config;
		}
		printf(", kernel %s", config->kernel);
		if (config->fdt)
			printf(", fdt %s", config->fdt);
		if (config->ramdisk)
			printf(", ramdisk %s", config->ramdisk);
		if (config->compat.name) {
			printf(", compat");
			int bytes = config->compat.size;
			const char *compat_str = config->compat.data;
			for (int pos = 0; bytes && compat_str[0]; pos++) {
				printf(" %s", compat_str);
				if (pos == config->compat_rank)
					printf(" (match)");
				int len = strlen(compat_str) + 1;
				compat_str += len;
				bytes -= len;
			}

			if ((compat_config && (config->compat_rank >
					       compat_config->compat_rank)) ||
			    (!compat_config && (config->compat_rank != -1)))
				compat_config = config;
		}
		printf("\n");
	}

	FitConfigNode *to_boot = NULL;
	if (compat_config) {
		to_boot = compat_config;
		printf("Choosing best match %s.\n", to_boot->name);
	} else if (default_config) {
		to_boot = default_config;
		printf("No match, choosing default %s.\n", to_boot->name);
	} else {
		printf("No compatible or default configs. Giving up.\n");
		// We're leaking memory here, but at this point we're beyond
		// saving anyway.
		return 1;
	}

	*kernel = to_boot->kernel_node->data;
	*kernel_size = to_boot->kernel_node->size;

	if (to_boot->fdt_node) {
		*dt = fdt_unflatten(to_boot->fdt_node->data);
		if (!*dt) {
			printf("Failed to unflatten the kernel's fdt.\n");
			return 1;
		}

		/* Update only if non-NULL cmd line */
		if (cmd_line)
			update_chosen(*dt, cmd_line);

		update_memory(*dt);
		if (to_boot->ramdisk_node)
			update_ramdisk(*dt, to_boot->ramdisk_node);
	}

	return 0;
}
