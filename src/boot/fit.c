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
	const char *compat;

	ListNode list_node;
} FitConfigNode;

static ListNode image_nodes;
static ListNode config_nodes;




static void image_node(DeviceTreeNode *node)
{
	FitImageNode *image = malloc(sizeof(FitImageNode));
	assert(image);
	memset(image, 0, sizeof(*image));
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
	FitConfigNode *config = malloc(sizeof(FitConfigNode));
	assert(config);
	memset(config, 0, sizeof(*config));

	config->name = node->name;

	DeviceTreeProperty *prop;
	list_for_each(prop, node->properties, list_node) {
		if (!strcmp("kernel", prop->prop.name))
			config->kernel = prop->prop.data;
		else if (!strcmp("fdt", prop->prop.name))
			config->fdt = prop->prop.data;
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

static const char *fdt_compat(void *blob, uint32_t start_offset)
{
	int offset = start_offset;
	int size;

	size = fdt_node_name(blob, offset, NULL);
	if (!size)
		return NULL;
	offset += size;

	FdtProperty prop;
	while ((size = fdt_next_property(blob, offset, &prop))) {
		if (!strcmp("compatible", prop.name))
			return prop.data;

		offset += size;
	}

	return NULL;
}

static void update_chosen(DeviceTreeNode *chosen, char *cmd_line)
{
	DeviceTreeProperty *bootargs = NULL;

	DeviceTreeProperty *prop;
	list_for_each(prop, chosen->properties, list_node)
		if (!strcmp("bootargs", prop->prop.name))
			bootargs = prop;

	if (!bootargs) {
		bootargs = dt_new_property();
		list_insert_after(&bootargs->list_node, &chosen->properties);
		bootargs->prop.name = "bootargs";
	}

	bootargs->prop.data = cmd_line;
	bootargs->prop.size = strlen(cmd_line) + 1;
}

static void update_reserve_map(uint64_t start, uint64_t end, void *data)
{
	DeviceTree *tree = (DeviceTree *)data;

	DeviceTreeReserveMapEntry *entry = dt_new_reserve_map_entry();
	entry->start = start;
	entry->size = end - start;

	list_insert_after(&entry->list_node, &tree->reserve_map);
}

static void count_entries(uint64_t start, uint64_t end, void *data)
{
	(*(int *)data)++;
}

typedef struct EntryParams
{
	unsigned addr_cells;
	unsigned size_cells;
	void *data;
} EntryParams;

static void update_mem_property(uint64_t start, uint64_t end, void *pdata)
{
	EntryParams *params = (EntryParams *)pdata;
	uint8_t *data = (uint8_t *)params->data;
	if (params->addr_cells == 2)
		*(uint64_t *)data = htobell(start);
	else
		*(uint32_t *)data = htobel(start);
	data += params->addr_cells * sizeof(uint32_t);
	if (params->size_cells == 2)
		*(uint64_t *)data = htobell(end - start);
	else
		*(uint32_t *)data = htobel(end - start);
	data += params->size_cells * sizeof(uint32_t);
	params->data = data;
}

static void update_memory(DeviceTree *tree, DeviceTreeNode *memory)
{
	DeviceTreeProperty *prop;
	unsigned addr_cells = 1;
	unsigned size_cells = 1;

	list_for_each(prop, tree->root->properties, list_node) {
		if (!strcmp("#address-cells", prop->prop.name))
			addr_cells = betohl(*(uint32_t *)prop->prop.data);
		if (!strcmp("#size-cells", prop->prop.name))
			size_cells = betohl(*(uint32_t *)prop->prop.data);
	}

	if (addr_cells > 2 || size_cells > 2) {
		printf("Bad cell count.\n");
		return;
	}

	Ranges mem;
	Ranges reserved;
	ranges_init(&mem);
	ranges_init(&reserved);

	for (int i = 0; i < lib_sysinfo.n_memranges; i++) {
		struct memrange *range = &lib_sysinfo.memrange[i];
		uint64_t start = range->base;
		uint64_t end = range->base + range->size;

		if (range->type == CB_MEM_RAM) {
			ranges_add(&mem, start, end);
		} else {
			ranges_add(&reserved, start, end);
		}
	}

	ranges_for_each(&reserved, &update_reserve_map, tree);

	DeviceTreeProperty *reg = NULL;
	list_for_each(prop, memory->properties, list_node)
		if (!strcmp("reg", prop->prop.name))
			reg = prop;

	if (!reg) {
		reg = dt_new_property();
		list_insert_after(&reg->list_node, &memory->properties);
		reg->prop.name = "reg";
	}

	int count = 0;
	ranges_for_each(&mem, &count_entries, &count);

	int entry_size = (addr_cells + size_cells) * sizeof(uint32_t);
	reg->prop.size = entry_size * count;
	void *data = malloc(reg->prop.size);
	assert(data);
	reg->prop.data = data;
	EntryParams params = { addr_cells, size_cells, data };
	ranges_for_each(&mem, &update_mem_property, &params);
}

static void update_kernel_dt(DeviceTree *tree, char *cmd_line)
{
	DeviceTreeNode *chosen = NULL;
	DeviceTreeNode *memory = NULL;

	DeviceTreeNode *node;
	list_for_each(node, tree->root->children, list_node) {
		if (!strcmp(node->name, "chosen"))
			chosen = node;
		else if (!strcmp(node->name, "memory"))
			memory = node;
	}

	if (!chosen) {
		chosen = dt_new_node();
		list_insert_after(&chosen->list_node, &tree->root->children);
		chosen->name = "chosen";
	}
	update_chosen(chosen, cmd_line);

	if (!memory) {
		memory = dt_new_node();
		list_insert_after(&memory->list_node, &tree->root->children);
		memory->name = "memory";
	}
	update_memory(tree, memory);
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
	const char *compat_config_name = CONFIG_KERNEL_FIT_COMPAT;
	FitConfigNode *compat_config = NULL;

	fit_unpack(tree, &default_config_name);

	// List the images we found.
	list_for_each(image, image_nodes, list_node)
		printf("Image %s has %d bytes.\n", image->name, image->size);

	// Process and list the configs.
	list_for_each(config, config_nodes, list_node) {
		if (config->kernel)
			config->kernel_node = find_image(config->kernel);
		if (config->fdt)
			config->fdt_node = find_image(config->fdt);

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
			config->compat = fdt_compat(fdt_blob, fdt_offset);
		}

		printf("Config %s, kernel %s", config->name, config->kernel);
		if (config->fdt)
			printf(", fdt %s", config->fdt);
		if (default_config_name &&
				!strcmp(config->name, default_config_name)) {
			printf(" (default)");
			default_config = config;
		}
		if (config->compat &&
				!strcmp(config->compat, compat_config_name)) {
			printf(" (compat)");
			compat_config = config;
		}
		printf("\n");
	}

	FitConfigNode *to_boot = NULL;
	if (default_config)
		to_boot = default_config;
	if (compat_config)
		to_boot = compat_config;

	if (!to_boot) {
		printf("No compatible or default configs. Giving up.\n");
		// We're leaking memory here, but at this point we're beyond
		// saving anyway.
		return 1;
	}

	printf("Choosing config %s.\n", to_boot->name);

	*kernel = to_boot->kernel_node->data;
	*kernel_size = to_boot->kernel_node->size;

	if (to_boot->fdt_node) {
		*dt = fdt_unflatten(to_boot->fdt_node->data);
		if (!*dt) {
			printf("Failed to unflatten the kernel's fdt.\n");
			return 1;
		}

		update_kernel_dt(*dt, cmd_line);
	}

	return 0;
}
