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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <libpayload.h>
#include <endian.h>

#include "base/init_funcs.h"
#include "base/device_tree.h"

#include "smc.h"

#define MAX_MEM_CHUNKS 2
#define TOS_NS_MEM_MAP_MAGIC_VALUE (0xfeedbeef)
#define TOS_NS_MEM_MAP_CUR_VERSION (0x1)

struct tos_ns_mem_map_entry_t {
	uint64_t base;
	uint64_t size;
};

struct __attribute__ ((__packed__)) tos_ns_mem_map_t {
	uint32_t magic;
	uint32_t ver;
	uint32_t num;
	uint8_t reserved[2];
	struct tos_ns_mem_map_entry_t mappings[MAX_MEM_CHUNKS];
};


static int nsmem_to_tos(DeviceTreeFixup *fixup, DeviceTree *tree)
{
	const char *memory_dt_name[] = { "memory", NULL };
	DeviceTreeNode *memory_node;
	size_t size;
	uint64_t *reg;
	uint64_t base64;
	uint64_t size64;
	struct tos_ns_mem_map_t ns_mem_map;
	int32_t ret;

	memory_node = dt_find_node(tree->root, memory_dt_name, NULL, NULL, 0);
	if (!memory_node) {
		printf("NSMEM: ERROR: Faied to find node /%s\n",
		       memory_dt_name[0]);
		return 1;
	}

	dt_find_bin_prop(memory_node, "reg", (void *)&reg, &size);
	if ((size != 16) && (size != 32)) {
		printf("NSMEM: ERROR: incorrect size(%ld)\n", size);
		return 1;
	}

	ns_mem_map.magic = TOS_NS_MEM_MAP_MAGIC_VALUE;
	ns_mem_map.ver = TOS_NS_MEM_MAP_CUR_VERSION;
	ns_mem_map.num = 0;
	ns_mem_map.reserved[0] = 0;
	ns_mem_map.reserved[1] = 0;

	while (size) {
		base64 = be64toh(*reg++);
		size64 = be64toh(*reg++);
		printf("NSMEM: base64=0x%llx size64=0x%llx\n", base64, size64);
		ns_mem_map.mappings[ns_mem_map.num].base = base64;
		ns_mem_map.mappings[ns_mem_map.num].size = size64;
		++ns_mem_map.num;
		size -= 16;
	}

	/* flush dcache */
	dcache_clean_by_mva((void *)&ns_mem_map, sizeof(ns_mem_map));
	ret = smc_call(SMC_TOS_REGISTER_NS_DRAM_RANGES, (uintptr_t)&ns_mem_map,
		 sizeof(ns_mem_map));

	if (ret) {
		printf("WARNING: Failed to pass NSMEM ranges to TOS (err=%d)\n",
		       ret);
		return 1;
	} else {
		printf("NSMEM ranges recorded by TOS\n");
	}

	return 0;
}

static DeviceTreeFixup nsmem_fixup = {
	.fixup = nsmem_to_tos,
};

static int nsmem_setup(void)
{
	list_insert_after(&nsmem_fixup.list_node, &device_tree_fixups);
	return 0;
}

INIT_FUNC(nsmem_setup);
