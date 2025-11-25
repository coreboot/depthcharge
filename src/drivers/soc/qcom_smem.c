#include <libpayload.h>
#include <string.h>
#include "drivers/soc/qcom_smem.h"
#include "drivers/soc/qcom_soc.h"

#ifndef HWIO_TCSR_TZ_WONCE_0_ADDR
#error "Undefined HWIO_TCSR_TZ_WONCE_0_ADDR. Please check if SMEM driver is supported for this SoC."
#endif

smem_info_type smem_info;

/* Currently only SMEM_APPS (index 0) partition is used for read-only access */
static smem_partition_info_type smem_info_prt[1];

static int smem_targ_init(void)
{
	smem_targ_info_type *smem_targ_info_ptr;
	smem_targ_info_type smem_targ_info_local;
	uint64_t smem_targ_info_addr;
	uint32_t wonce_0, wonce_1;

	wonce_0 = read32((void *)HWIO_TCSR_TZ_WONCE_0_ADDR);
	wonce_1 = read32((void *)HWIO_TCSR_TZ_WONCE_1_ADDR);
	smem_targ_info_addr = wonce_0 | ((uint64_t)wonce_1 << 32);

	if (smem_targ_info_addr == 0) {
		printf("SMEM: Invalid target info address\n");
		return -1;
	}

	smem_targ_info_ptr = (smem_targ_info_type *)phys_to_virt(smem_targ_info_addr);
	if (smem_targ_info_ptr == NULL) {
		printf("SMEM: Failed to map target info\n");
		return -1;
	}

	smem_targ_info_local = *smem_targ_info_ptr;

	if (smem_targ_info_local.identifier != SMEM_TARG_INFO_IDENTIFIER) {
		printf("SMEM: Target info identifier mismatch\n");
		return -1;
	}

	smem_info.smem_phys_base_addr = (uint8_t *)(uintptr_t)smem_targ_info_local.smem_base_phys_addr;
	smem_info.smem_size = smem_targ_info_local.smem_size;
	smem_info.max_items = smem_targ_info_local.smem_max_items;

	if (smem_info.max_items == 0)
		smem_info.max_items = SMEM_MAX_ITEMS;

	return 0;
}

static int smem_boot_init_check(void)
{
	volatile smem_static_allocs_type *static_allocs;

	if (!smem_info.smem_base_addr)
		return 0;

	static_allocs = (volatile smem_static_allocs_type *)smem_info.smem_base_addr;
	return (static_allocs->heap_info.initialized == SMEM_HEAP_INFO_INIT);
}

static int smem_part_process_item(smem_partition_info_type *info,
				  smem_alloc_params_type *params,
				  smem_partition_allocation_header_type **item_hdr,
				  uint16_t padding_header,
				  uint32_t *size_remaining,
				  int cached)
{
	uint32_t size_item;
	uint16_t smem_type;
	uint16_t canary;
	uint32_t size_total;
	uint32_t padding_data;
	int found = 0;

	canary = (*item_hdr)->canary;
	smem_type = (*item_hdr)->smem_type;
	size_item = (*item_hdr)->size;

	size_total = size_item + padding_header + sizeof(smem_partition_allocation_header_type);

	if (canary != SMEM_ALLOC_HDR_CANARY) {
		printf("SMEM: Invalid canary 0x%X at location 0x%p\n", canary, (void *)(*item_hdr));
		return 0;
	}

	if (size_item > info->size) {
		printf("SMEM: Invalid item size:%d, Partition size:%d smem_type:0x%04X\n",
		       size_item, info->size, smem_type);
		return 0;
	}

	if (size_total > *size_remaining) {
		printf("SMEM: Invalid item size. Total size: %d, remaining: %d\n",
		       size_total, *size_remaining);
		return 0;
	}

	*size_remaining -= size_total;

	if (smem_type == params->smem_type) {
		if (cached) {
			params->buffer = (void *)((size_t)(*item_hdr) - size_item);
			params->flags |= SMEM_ALLOC_FLAG_CACHED;
		} else {
			params->buffer = (void *)((size_t)(*item_hdr) + sizeof(smem_partition_allocation_header_type));
			params->flags &= ~SMEM_ALLOC_FLAG_CACHED;
		}

		padding_data = (*item_hdr)->padding_data;
		if (padding_data > size_item) {
			printf("SMEM: Invalid padding data size:%d, size_item:%d smem_type:0x%04X\n",
			       padding_data, size_item, smem_type);
			return 0;
		}

		params->size = size_item - padding_data;
		params->size = ROUND_UP(params->size, 8);
		found = 1;
	}

	if (cached) {
		*item_hdr = (smem_partition_allocation_header_type *)((size_t)(*item_hdr) - size_total);
	} else {
		*item_hdr = (smem_partition_allocation_header_type *)((size_t)(*item_hdr) + size_total);
	}

	return found;
}

static int32_t smem_part_get_addr_ex(smem_alloc_params_type *params)
{
	smem_partition_info_type *info;
	smem_partition_allocation_header_type *item_hdr;
	uint32_t size_remaining;
	uint32_t limit_addr;
	uint16_t padding_header;

	if (params == NULL) {
		printf("SMEM: Invalid params\n");
		return SMEM_STATUS_INVALID_PARAM;
	}

	/* Only SMEM_APPS partition (index 0) is supported */
	info = &smem_info_prt[0];

	if (info->size == 0) {
		params->buffer = NULL;
		return SMEM_STATUS_NOT_FOUND;
	}

	info->offset_free_cached = info->header->offset_free_cached;
	info->offset_free_uncached = info->header->offset_free_uncached;

	if ((info->offset_free_uncached > info->offset_free_cached) ||
	    (info->offset_free_uncached > info->size) ||
	    (info->offset_free_cached > info->size)) {
		printf("SMEM: Invalid heap pointers. Uncached: 0x%X, cached: 0x%X\n",
		       info->offset_free_uncached, info->offset_free_cached);
		return SMEM_STATUS_OUT_OF_RESOURCES;
	}

	size_remaining = info->size - sizeof(smem_partition_header_type);
	padding_header = 0;

	item_hdr = (smem_partition_allocation_header_type *)
		   ((uint32_t)(uintptr_t)info->header + sizeof(smem_partition_header_type));

	limit_addr = (uint32_t)(uintptr_t)info->header + info->offset_free_uncached;

	while ((uint32_t)(uintptr_t)item_hdr < limit_addr) {
		if (smem_part_process_item(info, params, &item_hdr, padding_header, &size_remaining, 0)) {
			return SMEM_STATUS_SUCCESS;
		}
	}

	padding_header = SMEM_PARTITION_ITEM_PADDING(info->size_cacheline);

	item_hdr = (smem_partition_allocation_header_type *)
		   ((uint32_t)(uintptr_t)info->header + info->size - padding_header -
		    sizeof(smem_partition_allocation_header_type));

	limit_addr = (uint32_t)(uintptr_t)info->header + info->offset_free_cached;

	while ((uint32_t)(uintptr_t)item_hdr >= limit_addr) {
		if (smem_part_process_item(info, params, &item_hdr, padding_header, &size_remaining, 1)) {
			return SMEM_STATUS_SUCCESS;
		}
	}

	return SMEM_STATUS_NOT_FOUND;
}

static void* smem_part_get_addr(uint32_t smem_type, uint32_t *buf_size)
{
	smem_alloc_params_type params;
	int32_t ret;

	if (buf_size == NULL) {
		printf("SMEM: Invalid buf_size\n");
		return NULL;
	}

	params.remote_host = SMEM_INVALID_HOST;
	params.smem_type = smem_type;
	params.size = 0;
	params.buffer = NULL;
	params.flags = SMEM_ALLOC_FLAG_NONE;

	ret = smem_part_get_addr_ex(&params);
	*buf_size = params.size;
	return (ret == SMEM_STATUS_SUCCESS) ? params.buffer : NULL;
}

static const smem_funcs_type smem_part_funcs = {
	.alloc = NULL,
	.get_addr = smem_part_get_addr,
	.alloc_ex = NULL,
	.get_addr_ex = NULL
};

static int smem_part_init(void)
{
	smem_alloc_params_type params;
	smem_toc_type *toc;
	uint8_t *smem_base;
	uint32_t smem_size;
	uint32_t i;
	uint32_t size;
	int32_t status;
	int ret = -1;

	smem_base = smem_info.smem_base_addr;
	smem_size = smem_info.smem_size;
	toc = (smem_toc_type *)(smem_base + smem_size - SMEM_TOC_SIZE);

	if (toc->identifier != SMEM_TOC_IDENTIFIER) {
		printf("SMEM: No TOC found\n");
		return -1;
	}

	if (toc->version != 1) {
		printf("SMEM: Unsupported TOC version %d\n", toc->version);
		return -1;
	}

	if (toc->num_entries > SMEM_MAX_PARTITIONS || toc->num_entries == 0) {
		printf("SMEM: Invalid TOC entries: %d (max %d)\n",
		       toc->num_entries, SMEM_MAX_PARTITIONS);
		return -1;
	}

	printf("SMEM: TOC found with %d partition entries\n", toc->num_entries);

	for (i = 0; i < toc->num_entries; i++) {
		smem_toc_entry_type *entry = &toc->entry[i];
		smem_host_type host0 = entry->host0;
		smem_host_type host1 = entry->host1;
		smem_host_type remote_host;

		if (host0 == SMEM_COMMON_HOST && host1 == SMEM_COMMON_HOST) {
			remote_host = SMEM_THIS_HOST;
		} else {
			/*
			 * Skip partitions not shared with APPS (SMEM_THIS_HOST).
			 * Depthcharge only needs read-only access to the COMMON partition
			 * (COMMON_HOST <-> COMMON_HOST) which contains platform information
			 * like chip serial number. Other partitions (e.g., APPS<->MODEM,
			 * APPS<->ADSP) are used for inter-processor communication and are
			 * not needed in the DC context.
			 */
			continue;
		}

		size = entry->size;

		if (size == 0)
			continue;

		uint32_t partition_offset = entry->offset;
		uint32_t size_cacheline = entry->size_cacheline;
		smem_partition_header_type *hdr;
		uint32_t part_size;

		hdr = (smem_partition_header_type *)(smem_base + partition_offset);

		if (hdr->identifier != SMEM_PARTITION_HEADER_ID) {
			printf("SMEM: Invalid partition header ID 0x%08x at offset 0x%x\n",
			       hdr->identifier, partition_offset);
			continue;
		}

		if (partition_offset == 0 || partition_offset >= (smem_size - SMEM_TOC_SIZE)) {
			printf("SMEM: Invalid partition offset 0x%x\n", partition_offset);
			continue;
		}

		part_size = hdr->size;
		if (part_size != size) {
			printf("SMEM: Partition size mismatch. Header:%d, TOC:%d, host0:0x%04X, host1:0x%04X\n",
			       part_size, size, host0, host1);
			continue;
		}

		smem_partition_info_type *prt = &smem_info_prt[0];

		prt->header = hdr;
		prt->size = size;
		prt->size_cacheline = size_cacheline;
		prt->offset_free_uncached = sizeof(smem_partition_header_type);
		prt->offset_free_cached = size;

		printf("SMEM: Partition initialized - host0:0x%04X, host1:0x%04X, size:0x%x, cacheline:%d\n",
		       host0, host1, size, size_cacheline);

		params.remote_host = remote_host;
		params.smem_type = SMEM_MEM_FIRST;
		params.size = 0;
		params.buffer = NULL;
		params.flags = 0;

		status = smem_part_get_addr_ex(&params);

		if (status != SMEM_STATUS_SUCCESS && status != SMEM_STATUS_NOT_FOUND) {
			printf("SMEM: Partition %d init failed with status 0x%x\n",
			       remote_host, status);
			continue;
		}

		printf("SMEM: Partition scan complete - uncached:0x%x, cached:0x%x\n",
		       prt->offset_free_uncached, prt->offset_free_cached);

		ret = 0;
		break;
	}

	if (ret != 0)
		printf("SMEM: No valid COMMON partition found\n");

	return ret;
}

static void smem_init(void)
{
	if (smem_info.state == SMEM_STATE_INITIALIZED)
		return;

	printf("SMEM: Initializing...\n");

	if (smem_targ_init() != 0)
		return;

	smem_info.smem_base_addr = (uint8_t *)phys_to_virt((uintptr_t)smem_info.smem_phys_base_addr);
	if (smem_info.smem_base_addr == NULL) {
		printf("SMEM: Failed to map SMEM region\n");
		return;
	}

	if (!smem_boot_init_check()) {
		printf("SMEM: Heap validation failed\n");
		return;
	}

	smem_info.version = SMEM_VERSION_ID;
	smem_info.funcs = &smem_part_funcs;

	if (smem_part_init() != 0)
		return;

	smem_info.state = SMEM_STATE_INITIALIZED;
	printf("SMEM: Initialization complete.\n");
}

static void *qcom_smem_get_addr(uint32_t smem_type, uint32_t *buf_size)
{
	if (smem_info.state != SMEM_STATE_INITIALIZED)
		smem_init();

	if (smem_info.state != SMEM_STATE_INITIALIZED || !smem_info.funcs ||
	    !smem_info.funcs->get_addr) {
		printf("SMEM: Not initialized or get_addr not supported\n");
		return NULL;
	}

	return smem_info.funcs->get_addr(smem_type, buf_size);
}

uint32_t qcom_smem_get_chip_serial(void)
{
	DalPlatformInfoSMemType *pSMem;
	uint32_t buf_size;

	pSMem = (DalPlatformInfoSMemType *)qcom_smem_get_addr(SMEM_HW_SW_BUILD_ID, &buf_size);

	if (!pSMem) {
		printf("SMEM: Failed to get platform info\n");
		return 0;
	}

	printf("SMEM: nChipSerial = 0x%08x\n", pSMem->nChipSerial);
	return pSMem->nChipSerial;
}
