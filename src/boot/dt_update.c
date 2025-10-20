// SPDX-License-Identifier: GPL-2.0

#include <assert.h>
#include <commonlib/device_tree.h>
#include <libpayload.h>
#include <lp_vboot.h>
#include <stdint.h>
#include <tlcl.h>
#include <ctype.h>

#include "base/ranges.h"
#include "base/timestamp.h"
#include "vboot/boot.h"
#include "vboot/stages.h"
#include "vboot/secdata_tpm.h"

void dt_update_chosen(struct device_tree *tree, const char *cmd_line)
{
	int ret;
	union {
		struct {
			u8 tpm_buf[64];
		};
		struct {
			uint64_t kaslr;
			uint8_t rng[56];
		};
	} *seed = xzalloc(sizeof(*seed));
	uint32_t size;
	static const char * const path[] = { "chosen", NULL };
	struct device_tree_node *node = dt_find_node(tree->root, path,
						     NULL, NULL, 1);

	/* Update only if non-NULL cmd line */
	if (cmd_line)
		dt_add_string_prop(node, "bootargs", cmd_line);

	if (CONFIG(MOCK_TPM))
		return;

	ret = TlclGetRandom(seed->tpm_buf, sizeof(seed->tpm_buf), &size);
	if (ret || size != sizeof(seed->tpm_buf)) {
		printf("TPM failed to populate kASLR seed buffer.\n");
		if (!vboot_in_recovery()) {
			struct vb2_context *ctx = vboot_get_context();
			vb2api_fail(ctx, VB2_RECOVERY_RW_TPM_R_ERROR, ret);
			vb2ex_commit_data(ctx);
			reboot();
		}
		/* In recovery we'd rather continue with a weak seed than risk
		   tripping up the kernel. We don't expect untrusted code to run
		   there anyway, so kernel exploits are less of a concern. */
	}
	timestamp_mix_in_randomness(seed->tpm_buf, sizeof(seed->tpm_buf));

	dt_add_u64_prop(node, "kaslr-seed", seed->kaslr);
	dt_add_bin_prop(node, "rng-seed", seed->rng, sizeof(seed->rng));
}

static void update_reserve_map(uint64_t start, uint64_t end, void *data)
{
	struct device_tree *tree = (struct device_tree *)data;

	struct device_tree_reserve_map_entry *entry = xzalloc(sizeof(*entry));
	entry->start = start;
	entry->size = end - start;

	list_insert_after(&entry->list_node, &tree->reserve_map);
}

typedef struct EntryParams {
	unsigned int addr_cells;
	unsigned int size_cells;
	void *data;
} EntryParams;

static uint64_t max_range(unsigned int size_cells)
{
	// Split up ranges who's sizes are too large to fit in #size-cells.
	// The largest value we can store isn't a power of two, so we'll round
	// down to make the math easier.
	return 0x1ULL << (size_cells * 32 - 1);
}

static void count_entries(u64 start, u64 end, void *pdata)
{
	EntryParams *params = (EntryParams *)pdata;
	unsigned int *count = (unsigned int *)params->data;
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

void dt_update_memory(struct device_tree *tree)
{
	Ranges mem;
	Ranges reserved;
	struct device_tree_node *node;
	u32 addr_cells = 2, size_cells = 1;
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
		 * Kernel likes its available memory areas at least 1MB
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
	unsigned int count = 0;
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

static int add_pkvm_drng_node(struct device_tree *tree, void *seed_addr, size_t seed_size)
{
	static const char *const reserved_mem[] = {"reserved-memory", "pkvm-drng-seed", NULL};

	uint32_t addr_cells, size_cells;
	uint64_t reg_addr = (uintptr_t)seed_addr;
	uint64_t reg_size = seed_size;
	struct device_tree_node *node;

	/* Get or create /reserved-memory/drng-seed node */
	node = dt_find_node(tree->root, reserved_mem, &addr_cells, &size_cells,
			    /* create */ 1);
	if (node == NULL) {
		printf("Failed to add pKVM DRNG seed to reserved-memory\n");
		return -1;
	}

	/* Add required node properties */
	dt_add_string_prop(node, "compatible", "google,pkvm-drng");
	dt_add_reg_prop(node, &reg_addr, &reg_size, 1, addr_cells, size_cells);
	dt_add_bin_prop(node, "no-map", NULL, 0);

	return 0;
}

#define ANDROID_PKVM_DRNG_SEED_SIZE 128

int dt_add_pkvm_drng(struct device_tree *tree, struct boot_info *bi)
{
	uint8_t *seed_buf;
	uint32_t status;

	timestamp_add_now(TS_PKVM_DRNG_SEED_START);

	/* Put seed after the pvmfw and align to page */
	seed_buf = (uint8_t *)ALIGN_UP((uintptr_t)bi->pvmfw_addr + bi->pvmfw_size, 4096);
	/* Make sure there's enough space left in pvmfw buffer */
	if (((uintptr_t)seed_buf - (uintptr_t)bi->pvmfw_addr) + ANDROID_PKVM_DRNG_SEED_SIZE >
	    bi->pvmfw_buffer_size) {
		printf("No enough space for pKVM DRNG seed in pvmfw buffer\n");
		return -1;
	}

	status = secdata_generate_randomness(seed_buf, ANDROID_PKVM_DRNG_SEED_SIZE);
	if (status != TPM_SUCCESS) {
		printf("Failed to generate pKVM DRNG seed\n");
		return -1;
	}

	timestamp_add_now(TS_PKVM_DRNG_SEED_DONE);

	return add_pkvm_drng_node(tree, seed_buf, ANDROID_PKVM_DRNG_SEED_SIZE);
}
