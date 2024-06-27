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
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __BOOT_FIT_H__
#define __BOOT_FIT_H__

#include <commonlib/list.h>
#include <stddef.h>
#include <stdint.h>

#include "base/device_tree.h"

typedef enum CompressionType
{
	CompressionInvalid,
	CompressionNone,
	CompressionLzma,
	CompressionLz4,
} CompressionType;

typedef struct FitImageNode
{
	const char *name;
	void *data;
	uint32_t size;
	CompressionType compression;

	struct list_node list_node;
} FitImageNode;

typedef struct FitConfigNode
{
	const char *name;
	FitImageNode *kernel;
	FitImageNode *fdt;
	struct list_node overlays;
	FitImageNode *ramdisk;
	struct fdt_property compat;
	int compat_rank;
	int compat_pos;

	struct list_node list_node;
} FitConfigNode;

/*
 * The same overlay may be used in more than one config node, so the
 * list nodes to chain them MUST be in separate structs that get
 * allocated per config. They can not be embedded in the overlay's
 * FitImageNode struct like it's common in depthcharge code.
 */
typedef struct FitOverlayChain
{
	FitImageNode *overlay;
	struct list_node list_node;
} FitOverlayChain;

/*
 * Unpack a FIT image into memory, choosing the right configuration through the
 * compatible string set by fit_add_compat() and unflattening the corresponding
 * kernel device tree.
 */
FitImageNode *fit_load(void *fit, char *cmd_line, struct device_tree **dt);

/*
 * Add a compatible string for the preferred kernel DT to the list for this
 * platform. This should be called before the first fit_load() so it will be
 * ranked as a better match than the default compatible strings. |compat| must
 * stay accessible throughout depthcharge's runtime (i.e. not stack-allocated)!
 */
void fit_add_compat(const char *compat);

void fit_add_ramdisk(struct device_tree *tree, void *ramdisk_addr,
		     size_t ramdisk_size);

size_t fit_decompress(FitImageNode *node, void *buffer, size_t bufsize);

#endif /* __BOOT_FIT_H__ */
