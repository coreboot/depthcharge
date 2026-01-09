/*
 * Copyright 2013 Google LLC
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
#include <lp_vboot.h>
#include <lzma.h>
#include <lz4.h>
#include <stdint.h>
#include <tlcl.h>
#include <ctype.h>

#include "base/ranges.h"
#include "base/timestamp.h"
#include "boot/fit.h"
#include "drivers/power/power.h"
#include "image/symbols.h"
#include "vboot/stages.h"



static struct list_node image_nodes;
static struct list_node config_nodes;

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
	char compat[256];
	char base[256];
	char sku_string[16];
	char rev_string[16];

	static int done = 0;
	if (done)
		return;
	done = 1;

	int ret;
	struct cb_mainboard *mainboard =
		phys_to_virt(lib_sysinfo.cb_mainboard);
	const char *mb_part_string = cb_mb_part_string(mainboard);
	ret = snprintf(base, sizeof(base), "google,%s", mb_part_string);
	assert(ret < sizeof(base));
	sprintf(rev_string, "-rev%u", rev);
	sprintf(sku_string, "-sku%u", sku);

	char *c;
	for (c = base; *c != '\0'; c++) {
		if (*c == '_')
			*c = '-';
		else
			*c = tolower(*c);
	}

	if (sku != UNDEFINED_STRAPPING_ID && rev != UNDEFINED_STRAPPING_ID) {
		ret = snprintf(compat, sizeof(compat),
			       "%s%s%s", base, rev_string, sku_string);
		assert(ret < sizeof(compat));
		fit_add_compat(strdup(compat));
	}

	if (rev != UNDEFINED_STRAPPING_ID) {
		ret = snprintf(compat, sizeof(compat),
			       "%s%s", base, rev_string);
		assert(ret < sizeof(compat));
		fit_add_compat(strdup(compat));
	}

	if (sku != UNDEFINED_STRAPPING_ID) {
		ret = snprintf(compat, sizeof(compat),
			       "%s%s", base, sku_string);
		assert(ret < sizeof(compat));
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
	printf("ERROR: Can't find image node %s!\n", name);
	return NULL;
}

static FitImageNode *find_image_with_overlays(const char *name, int bytes,
					      struct list_node *prev)
{
	FitImageNode *base = find_image(name);
	if (!base)
		return NULL;

	int len = strnlen(name, bytes) + 1;
	bytes -= len;
	name += len;
	while (bytes > 0) {
		FitOverlayChain *next = xzalloc(sizeof(*next));
		next->overlay = find_image(name);
		if (!next->overlay)
			return NULL;
		list_insert_after(&next->list_node, prev);
		prev = &next->list_node;
		len = strnlen(name, bytes) + 1;
		bytes -= len;
		name += len;
	}

	return base;
}

static void image_node(struct device_tree_node *node)
{
	FitImageNode *image = xzalloc(sizeof(*image));
	image->compression = CompressionNone;

	image->name = node->name;

	struct device_tree_property *prop;
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

static void config_node(struct device_tree_node *node)
{
	FitConfigNode *config = xzalloc(sizeof(*config));
	config->name = node->name;

	struct device_tree_property *prop;
	list_for_each(prop, node->properties, list_node) {
		if (!strcmp("kernel", prop->prop.name))
			config->kernel = find_image(prop->prop.data);
		else if (!strcmp("fdt", prop->prop.name))
			config->fdt = find_image_with_overlays(prop->prop.data,
				prop->prop.size, &config->overlays);
		else if (!strcmp("ramdisk", prop->prop.name))
			config->ramdisk = find_image(prop->prop.data);
		else if (!strcmp("compatible", prop->prop.name))
			config->compat = prop->prop;
	}

	list_insert_after(&config->list_node, &config_nodes);
}

static void fit_unpack(struct device_tree *tree, const char **default_config)
{
	struct device_tree_node *child;
	struct device_tree_node *images = dt_find_node_by_path(tree, "/images",
							       NULL, NULL, 0);
	if (images)
		list_for_each(child, images->children, list_node)
			image_node(child);

	struct device_tree_node *configs = dt_find_node_by_path(tree,
		"/configurations", NULL, NULL, 0);
	if (configs) {
		*default_config = dt_find_string_prop(configs, "default");
		list_for_each(child, configs->children, list_node)
			config_node(child);
	}
}

static int fdt_find_compat(void *blob, uint32_t start_offset,
			   struct fdt_property *prop)
{
	int offset = start_offset;
	int size;

	size = fdt_next_node_name(blob, offset, NULL);
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

static int fit_check_compat(struct fdt_property *compat_prop,
			    const char *compat_name)
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
		struct fdt_header *fdt_header = fdt_blob;
		uint32_t fdt_offset =
			betohl(fdt_header->structure_offset);

		if (config->fdt->compression != CompressionNone) {
			printf("ERROR: config %s has a compressed FDT without "
			       "external compatible property, skipping.\n",
			       config->name);
			return -1;
		}

		// FDT overlays are not supported in legacy FIT images.
		if (config->overlays.next) {
			printf("ERROR: config %s has overlay but no compat!\n",
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
	case CompressionLegacyLz4:
		printf("Legacy LZ4 decompressing %s to %p\n", node->name, buffer);
		return ulz4ln(node->data, node->size, buffer, bufsize);
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
	size_t size = _fit_fdt_end - _fit_fdt_start;

	size = fit_decompress(fdt, buffer, size);
	if (!size)
		return NULL;

	void *ret = malloc(size);
	if (ret)
		memcpy(ret, buffer, size);
	return ret;
}

void fit_add_ramdisk(struct device_tree *tree, void *ramdisk_addr,
		     size_t ramdisk_size)
{
	const char *path[] = { "chosen", NULL };
	struct device_tree_node *node = dt_find_node(tree->root, path,
						     NULL, NULL, 1);

	/* Warning: this assumes the ramdisk is currently located below 4GiB. */
	u32 start = (uintptr_t)ramdisk_addr;
	u32 end = start + ramdisk_size;

	dt_add_u32_prop(node, "linux,initrd-start", start);
	dt_add_u32_prop(node, "linux,initrd-end", end);
}

int fit_add_pvmfw(struct device_tree *tree, void *pvmfw_addr, size_t pvmfw_size)
{
	if (!dt_add_reserved_memory_region(tree, "pkvm_guest_firmware",
			 "linux,pkvm-guest-firmware-memory",
			 (uintptr_t)pvmfw_addr, pvmfw_size, true))
		return -1;
	return 0;
}

FitImageNode *fit_load(void *fit, struct device_tree **dt)
{
	FitImageNode *image;
	FitConfigNode *config;
	FitOverlayChain *overlay_chain;

	printf("Loading FIT.\n");

	struct device_tree *tree = fdt_unflatten(fit);
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
		list_for_each(overlay_chain, config->overlays, list_node)
			printf(" %s", overlay_chain->overlay->name);
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

	list_for_each(overlay_chain, to_boot->overlays, list_node) {
		fdt_data = get_fdt_data(overlay_chain->overlay);
		if (!fdt_data) {
			printf("ERROR: Can't decompress overlay %s!\n",
			       overlay_chain->overlay->name);
			return NULL;
		}
		struct device_tree *overlay = fdt_unflatten(fdt_data);
		if (!overlay) {
			printf("ERROR: Can't unflatten overlay %s!\n",
			       overlay_chain->overlay->name);
			return NULL;
		}
		if (dt_apply_overlay(*dt, overlay) != 0) {
			printf("ERROR: Failed to apply overlay %s!\n",
			       overlay_chain->overlay->name);
			return NULL;
		}
	}

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
