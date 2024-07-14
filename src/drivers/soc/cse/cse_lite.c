/*
 * Copyright 2024 Google LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <cbfs.h>
#include <commonlib/region.h>
#include <libpayload.h>
#include <image/fmap.h>

#include "base/elog.h"
#include "base/init_funcs.h"
#include "cse_internal.h"
#include "cse.h"
#include "cse_layout.h"
#include "cse_lite.h"
#include "drivers/flash/flash.h"
#include "vboot/ui.h"

static const char * const cse_regions[] = {"RO", "RW"};

static struct get_bp_info_rsp cse_bp_info_rsp;

static int cse_is_subregion(const struct region *parent, size_t child_offset,
	size_t child_size)
{
	if (parent == NULL)
		return -1;
	size_t end = parent->offset + parent->size - 1;
	size_t child_start = parent->offset + child_offset;
	size_t child_end = child_start + child_size - 1;
	if (end < child_end)
		return -1;
	return 0;
}

static int cse_get_effective_region(const struct region *parent,
	const struct region *child, struct region *effective_region)
{
	if (parent == NULL || child == NULL || effective_region == NULL)
		return -1;
	if (cse_is_subregion(parent, child->offset, child->size)) {
		return -1;
	}
	effective_region->offset = parent->offset + child->offset;
	effective_region->size = child->size;
	return 0;
}

static ssize_t cse_region_readat(const struct region *rg, void *b,
	size_t offset, size_t size)
{
	if (cse_is_subregion(rg, offset, size))
		return -1;
	if (flash_read(b, rg->offset + offset, size) != size)
		return -1;
	return size;
}

static ssize_t cse_region_writeat(const struct region *rg, const void *b,
	size_t offset, size_t size)
{
	if (cse_is_subregion(rg, offset, size))
		return -1;
	if (flash_write(b, rg->offset + offset, size) != size)
		return -1;
	return size;
}

static ssize_t cse_region_eraseat(const struct region *rg,
	size_t offset, size_t size)
{
	if (cse_is_subregion(rg, offset, size))
		return -1;
	if (flash_erase(rg->offset + offset, size) != size)
		return -1;
	return size;
}

const struct region *boot_device_ro(void)
{
	static struct region flash_area;
	flash_area.size = lib_sysinfo.spi_flash.size;
	flash_area.offset = 0;
	return &flash_area;
}

const struct region *boot_device_rw(void)
{
	return boot_device_ro();
}

static int boot_device_rw_subregion(struct region *child,
	struct region *effective_region)
{
	return cse_get_effective_region(boot_device_rw(), child, effective_region);
}

static int fmap_locate_area_as_region_rw(const char *name, struct region *area)
{
	FmapArea ar;
	struct region child;

	if (fmap_find_area(name, &ar)) {
		printf("%s: couldn't find %s region\n", __func__, name);
		return -1;
	}
	child.offset = ar.offset;
	child.size = ar.size;
	return boot_device_rw_subregion(&child, area);
}

static const struct cse_bp_info *cse_get_bp_info_from_rsp(void)
{
	return &cse_bp_info_rsp.bp_info;
}

static uint8_t cse_get_current_bp(void)
{
	const struct cse_bp_info *cse_bp_info = cse_get_bp_info_from_rsp();
	return cse_bp_info->current_bp;
}

static const struct cse_bp_entry *cse_get_bp_entry(enum boot_partition_id bp)
{
	const struct cse_bp_info *cse_bp_info = cse_get_bp_info_from_rsp();
	return &cse_bp_info->bp_entries[bp];
}

static void cse_print_boot_partition_info(void)
{
	const struct cse_bp_entry *cse_bp;
	const struct cse_bp_info *cse_bp_info = cse_get_bp_info_from_rsp();

	printk(BIOS_DEBUG, "cse_lite: Number of partitions = %d\n",
			cse_bp_info->total_number_of_bp);
	printk(BIOS_DEBUG, "cse_lite: Current partition = %s\n",
			GET_BP_STR(cse_bp_info->current_bp));
	printk(BIOS_DEBUG, "cse_lite: Next partition = %s\n", GET_BP_STR(cse_bp_info->next_bp));
	printk(BIOS_DEBUG, "cse_lite: Flags = 0x%x\n", cse_bp_info->flags);

	/* Log version info of RO & RW partitions */
	cse_bp = cse_get_bp_entry(RO);
	if (cse_bp->status == BP_STATUS_SUCCESS)
		printk(BIOS_DEBUG, "cse_lite: %s version = %d.%d.%d.%d (Start=0x%x, End=0x%x)\n",
			GET_BP_STR(RO), cse_bp->fw_ver.major, cse_bp->fw_ver.minor,
			cse_bp->fw_ver.hotfix, cse_bp->fw_ver.build,
			cse_bp->start_offset, cse_bp->end_offset);
	else
		printk(BIOS_ERR, "cse_lite: %s status-0x%x\n", GET_BP_STR(RO), cse_bp->status);

	cse_bp = cse_get_bp_entry(RW);
	if (cse_bp->status == BP_STATUS_SUCCESS)
		printk(BIOS_DEBUG, "cse_lite: %s version = %d.%d.%d.%d (Start=0x%x, End=0x%x)\n",
			GET_BP_STR(RW), cse_bp->fw_ver.major, cse_bp->fw_ver.minor,
			cse_bp->fw_ver.hotfix, cse_bp->fw_ver.build,
			cse_bp->start_offset, cse_bp->end_offset);
	else
		printk(BIOS_ERR, "cse_lite: %s status-0x%x\n", GET_BP_STR(RW), cse_bp->status);
}

/*
 * Checks prerequisites for MKHI_BUP_COMMON_GET_BOOT_PARTITION_INFO and
 * MKHI_BUP_COMMON_SET_BOOT_PARTITION_INFO HECI commands.
 * It allows execution of the Boot Partition commands in below scenarios:
 *	- When CSE boots from RW partition (COM: Normal and CWS: Normal)
 *	- When CSE boots from RO partition (COM: Soft Temp Disable and CWS: Normal)
 *	- After HMRFPO_ENABLE command is issued to CSE (COM: SECOVER_MEI_MSG and CWS: Normal)
 * The prerequisite check should be handled in cse_get_bp_info() and
 * cse_set_next_boot_partition() since the CSE's current operation mode is changed between these
 * cmd handler calls.
 */
static bool cse_is_bp_cmd_info_possible(void)
{
	if (cse_is_hfs1_cws_normal()) {
		if (cse_is_hfs1_com_normal())
			return true;
		if (cse_is_hfs1_com_secover_mei_msg())
			return true;
		if (cse_is_hfs1_com_soft_temp_disable())
			return true;
	}
	return false;
}

static bool is_cse_bp_info_valid(struct get_bp_info_rsp *bp_info_rsp)
{
	/*
	 * In case the cse_bp_info_rsp header group ID, command is incorrect or is_resp is 0,
	 * then return false to indicate cse_bp_info is not valid.
	 */
	return (bp_info_rsp->hdr.group_id != MKHI_GROUP_ID_BUP_COMMON ||
		bp_info_rsp->hdr.command != MKHI_BUP_COMMON_GET_BOOT_PARTITION_INFO ||
		!bp_info_rsp->hdr.is_resp) ? false : true;
}

static enum cb_err cse_get_bp_info(void)
{
	struct get_bp_info_req {
		struct mkhi_hdr hdr;
		uint8_t reserved[4];
	} __packed;

	struct get_bp_info_req info_req = {
		.hdr.group_id = MKHI_GROUP_ID_BUP_COMMON,
		.hdr.command = MKHI_BUP_COMMON_GET_BOOT_PARTITION_INFO,
		.reserved = {0},
	};

	/*
	 * Check if the global cse bp info response stored in global cse_bp_info_rsp is
	 * valid. In case, it is not valid, continue to send the GET_BOOT_PARTITION_INFO
	 * command, else return.
	 */
	if (is_cse_bp_info_valid(&cse_bp_info_rsp))
		return CB_SUCCESS;

	if (!cse_is_bp_cmd_info_possible()) {
		printk(BIOS_ERR, "cse_lite: CSE does not meet prerequisites\n");
		return CB_ERR;
	}

	size_t resp_size = sizeof(struct get_bp_info_rsp);

	if (heci_send_receive(&info_req, sizeof(info_req), &cse_bp_info_rsp, &resp_size,
			HECI_MKHI_ADDR)) {
		printk(BIOS_ERR, "cse_lite: Could not get partition info\n");
		return CB_ERR;
	}

	if (cse_bp_info_rsp.hdr.result) {
		printk(BIOS_ERR, "cse_lite: Get partition info resp failed: %d\n",
				cse_bp_info_rsp.hdr.result);
		return CB_ERR;
	}

	cse_print_boot_partition_info();
	return CB_SUCCESS;
}

void cse_fill_bp_info(void)
{
	if (vboot_recovery_mode_enabled())
		return;

	if (cse_get_bp_info() != CB_SUCCESS)
		cse_trigger_vboot_recovery(CSE_COMMUNICATION_ERROR);
}

/*
 * It sends HECI command to notify CSE about its next boot partition. When payload wants
 * CSE to boot from certain partition (BP1 <RO> or BP2 <RW>), then this command can be used.
 * The CSE's valid bootable partitions are BP1(RO) and BP2(RW).
 * This function must be used before EOP.
 * Returns false on failure and true on success.
 */
static enum cb_err cse_set_next_boot_partition(enum boot_partition_id bp)
{
	struct set_boot_partition_info_req {
		struct mkhi_hdr hdr;
		uint8_t next_bp;
		uint8_t reserved[3];
	} __packed;

	struct set_boot_partition_info_req switch_req = {
		.hdr.group_id = MKHI_GROUP_ID_BUP_COMMON,
		.hdr.command = MKHI_BUP_COMMON_SET_BOOT_PARTITION_INFO,
		.next_bp = bp,
		.reserved = {0},
	};

	if (bp != RO && bp != RW) {
		printk(BIOS_ERR, "cse_lite: Incorrect partition id(%d) is provided", bp);
		return CB_ERR_ARG;
	}

	printk(BIOS_INFO, "cse_lite: Set Boot Partition Info Command (%s)\n", GET_BP_STR(bp));

	if (!cse_is_bp_cmd_info_possible()) {
		printk(BIOS_ERR, "cse_lite: CSE does not meet prerequisites\n");
		return CB_ERR;
	}

	struct mkhi_hdr switch_resp;
	size_t sw_resp_sz = sizeof(struct mkhi_hdr);

	if (heci_send_receive(&switch_req, sizeof(switch_req), &switch_resp, &sw_resp_sz,
			HECI_MKHI_ADDR))
		return CB_ERR;

	if (switch_resp.result) {
		printk(BIOS_ERR, "cse_lite: Set Boot Partition Info Response Failed: %d\n",
				switch_resp.result);
		return CB_ERR;
	}

	return CB_SUCCESS;
}

static enum cb_err cse_data_clear_request(void)
{
	struct data_clr_request {
		struct mkhi_hdr hdr;
		uint8_t reserved[4];
	} __packed;

	struct data_clr_request data_clr_rq = {
		.hdr.group_id = MKHI_GROUP_ID_BUP_COMMON,
		.hdr.command = MKHI_BUP_COMMON_DATA_CLEAR,
		.reserved = {0},
	};

	if (!cse_is_hfs1_cws_normal() || !cse_is_hfs1_com_soft_temp_disable() ||
			cse_get_current_bp() != RO) {
		printk(BIOS_ERR, "cse_lite: CSE doesn't meet DATA CLEAR cmd prerequisites\n");
		return CB_ERR;
	}

	printk(BIOS_DEBUG, "cse_lite: Sending DATA CLEAR HECI command\n");

	struct mkhi_hdr data_clr_rsp;
	size_t data_clr_rsp_sz = sizeof(data_clr_rsp);

	if (heci_send_receive(&data_clr_rq, sizeof(data_clr_rq), &data_clr_rsp,
			&data_clr_rsp_sz, HECI_MKHI_ADDR)) {
		return CB_ERR;
	}

	if (data_clr_rsp.result) {
		printk(BIOS_ERR, "cse_lite: CSE DATA CLEAR command response failed: %d\n",
				data_clr_rsp.result);
		return CB_ERR;
	}

	return CB_SUCCESS;
}

static void cse_fw_update_misc_oper(void)
{
	struct ui_context ui;
	struct vb2_context *ctx = vboot_get_context();
	if (ui_init_context(&ui, ctx, UI_SCREEN_FIRMWARE_SYNC) == VB2_SUCCESS) {
		ui_display(&ui, NULL);
		elog_add_event_byte(ELOG_TYPE_FW_LATE_SOL, ELOG_FW_LATE_SOL_CSE_SYNC);
	}
}

/* Set the CSE's next boot partition and issues system reset */
static enum cb_err cse_set_and_boot_from_next_bp(enum boot_partition_id bp)
{
	if (cse_set_next_boot_partition(bp) != CB_SUCCESS)
		return CB_ERR;

	/* If board does not perform the reset, then perform global_reset */
	do_global_reset();

	die("cse_lite: Failed to reset the system\n");

	/* Control never reaches here */
	return CB_ERR;
}

static enum cb_err cse_boot_to_rw(void)
{
	if (cse_get_current_bp() == RW)
		return CB_SUCCESS;

	return cse_set_and_boot_from_next_bp(RW);
}

/* Check if CSE RW data partition is valid or not */
static bool cse_is_rw_dp_valid(void)
{
	const struct cse_bp_entry *rw_bp;

	rw_bp = cse_get_bp_entry(RW);
	return rw_bp->status != BP_STATUS_DATA_FAILURE;
}

/*
 * It returns true if RW partition doesn't indicate BP_STATUS_DATA_FAILURE
 * otherwise false if any operation fails.
 */
static enum cb_err cse_fix_data_failure_err(void)
{
	/*
	 * If RW partition status indicates BP_STATUS_DATA_FAILURE,
	 *  - Send DATA CLEAR HECI command to CSE
	 *  - Send SET BOOT PARTITION INFO(RW) command to set CSE's next partition
	 *  - Issue GLOBAL RESET HECI command.
	 */
	if (cse_is_rw_dp_valid())
		return CB_SUCCESS;

	if (cse_data_clear_request() != CB_SUCCESS)
		return CB_ERR;

	return cse_boot_to_rw();
}

static const struct fw_version *cse_get_bp_entry_version(enum boot_partition_id bp)
{
	const struct cse_bp_entry *cse_bp;

	cse_bp = cse_get_bp_entry(bp);
	return &cse_bp->fw_ver;
}

static const struct fw_version *cse_get_rw_version(void)
{
	return cse_get_bp_entry_version(RW);
}

static void cse_get_bp_entry_range(enum boot_partition_id bp, uint32_t *start_offset,
		uint32_t *end_offset)
{
	const struct cse_bp_entry *cse_bp;

	cse_bp = cse_get_bp_entry(bp);

	if (start_offset)
		*start_offset = cse_bp->start_offset;

	if (end_offset)
		*end_offset = cse_bp->end_offset;
}

static bool cse_is_rw_bp_status_valid(void)
{
	const struct cse_bp_entry *rw_bp;

	rw_bp = cse_get_bp_entry(RW);

	if (rw_bp->status == BP_STATUS_PARTITION_NOT_PRESENT ||
			rw_bp->status == BP_STATUS_GENERAL_FAILURE) {
		printk(BIOS_ERR, "cse_lite: RW BP (status:%u) is not valid\n", rw_bp->status);
		return false;
	}
	return true;
}

static enum cb_err cse_boot_to_ro(void)
{
	if (cse_get_current_bp() == RO)
		return CB_SUCCESS;

	return cse_set_and_boot_from_next_bp(RO);
}

static enum cb_err cse_get_rw_region(struct region *area)
{
	if (fmap_locate_area_as_region_rw(SOC_INTEL_CSE_FMAP_NAME, area) < 0) {
		printk(BIOS_ERR, "cse_lite: Failed to locate %s in FMAP\n",
				SOC_INTEL_CSE_FMAP_NAME);
		return CB_ERR;
	}

	return CB_SUCCESS;
}

static bool cse_is_rw_bp_sign_valid(const struct region *target_region)
{
	uint32_t cse_bp_sign;

	if (cse_region_readat(target_region, &cse_bp_sign, 0, CSE_RW_SIGN_SIZE) != CSE_RW_SIGN_SIZE) {
		printk(BIOS_ERR, "cse_lite: Failed to read RW boot partition signature\n");
		return false;
	}

	return cse_bp_sign == CSE_RW_SIGNATURE;
}

static enum cb_err cse_get_target_region(struct region *target_region)
{
	struct region cse_region_all;
	size_t size;
	uint32_t start_offset;
	uint32_t end_offset;

	if (cse_get_rw_region(&cse_region_all) != CB_SUCCESS)
		return CB_ERR;

	cse_get_bp_entry_range(RW, &start_offset, &end_offset);
	size = end_offset + 1 - start_offset;
	struct region sub_region = {.offset = start_offset, .size = size};
	if(cse_get_effective_region(&cse_region_all, &sub_region, target_region))
		return CB_ERR;

	printk(BIOS_DEBUG, "cse_lite: CSE RW partition: offset = 0x%x, size = 0x%x\n",
			(uint32_t)start_offset, (uint32_t)size);

	return CB_SUCCESS;
}

/*
 * Compare versions of CSE CBFS sub-component and CSE sub-component partition
 * In case of CSE component comparison:
 * If ver_cmp_status = 0, no update is required
 * If ver_cmp_status < 0, payload downgrades CSE RW region
 * If ver_cmp_status > 0, payload upgrades CSE RW region
 */
static int cse_compare_sub_part_version(const struct fw_version *a, const struct fw_version *b)
{
	if (a->major != b->major)
		return a->major - b->major;
	else if (a->minor != b->minor)
		return a->minor - b->minor;
	else if (a->hotfix != b->hotfix)
		return a->hotfix - b->hotfix;
	else
		return a->build - b->build;
}

static enum cb_err cse_erase_rw_region(const struct region *target_region)
{
	if (cse_region_eraseat(target_region, 0, target_region->size) < 0) {
		printk(BIOS_ERR, "cse_lite: CSE RW partition could not be erased\n");
		return CB_ERR;
	}
	return CB_SUCCESS;
}

static enum cb_err cse_copy_rw(const struct region *target_region, const void *buf,
		size_t offset, size_t size)
{
	if (cse_region_writeat(target_region, buf, offset, size) < 0) {
		printk(BIOS_ERR, "cse_lite: Failed to update CSE firmware\n");
		return CB_ERR;
	}

	return CB_SUCCESS;
}

static bool read_ver_field(const char *start, char **curr, size_t size, uint16_t *ver_field)
{
	if ((*curr - start) >= size) {
		printk(BIOS_ERR, "cse_lite: Version string read overflow!\n");
		return false;
	}

	*ver_field = skip_atoi(curr);
	(*curr)++;
	return true;
}

static enum cb_err get_cse_ver_from_cbfs(struct fw_version *cbfs_rw_version)
{
	char *version_str, *cbfs_ptr;
	size_t size;

	if (cbfs_rw_version == NULL)
		return CB_ERR;

	cbfs_ptr = cbfs_map(SOC_INTEL_CSE_RW_VERSION_CBFS_NAME, &size);
	version_str = cbfs_ptr;
	if (!version_str) {
		printk(BIOS_ERR, "cse_lite: Failed to get %s\n",
				SOC_INTEL_CSE_RW_VERSION_CBFS_NAME);
		return CB_ERR;
	}

	if (!read_ver_field(version_str, &cbfs_ptr, size, &cbfs_rw_version->major) ||
		!read_ver_field(version_str, &cbfs_ptr, size, &cbfs_rw_version->minor) ||
		!read_ver_field(version_str, &cbfs_ptr, size, &cbfs_rw_version->hotfix) ||
		!read_ver_field(version_str, &cbfs_ptr, size, &cbfs_rw_version->build)) {
		cbfs_unmap(version_str);
		return CB_ERR;
	}

	cbfs_unmap(version_str);
	return CB_SUCCESS;
}

static enum cse_update_status cse_check_update_status(struct region *target_region)
{
	int ret;
	struct fw_version cbfs_rw_version;

	if (!cse_is_rw_bp_sign_valid(target_region))
		return CSE_UPDATE_CORRUPTED;

	if (get_cse_ver_from_cbfs(&cbfs_rw_version) == CB_ERR)
		return CSE_UPDATE_METADATA_ERROR;

	printk(BIOS_DEBUG, "cse_lite: CSE CBFS RW version : %d.%d.%d.%d\n",
			cbfs_rw_version.major,
			cbfs_rw_version.minor,
			cbfs_rw_version.hotfix,
			cbfs_rw_version.build);

	ret = cse_compare_sub_part_version(&cbfs_rw_version, cse_get_rw_version());
	if (ret == 0)
		return CSE_UPDATE_NOT_REQUIRED;
	else if (ret < 0)
		return CSE_UPDATE_DOWNGRADE;
	else
		return CSE_UPDATE_UPGRADE;
}

static enum cb_err cse_write_rw_region(const struct region *target_region,
	const void *cse_cbfs_rw, const size_t cse_cbfs_rw_sz)
{
	/* Points to CSE CBFS RW image after boot partition signature */
	uint8_t *cse_cbfs_rw_wo_sign = (uint8_t *)cse_cbfs_rw + CSE_RW_SIGN_SIZE;

	/* Size of CSE CBFS RW image without boot partition signature */
	uint32_t cse_cbfs_rw_wo_sign_sz = cse_cbfs_rw_sz - CSE_RW_SIGN_SIZE;

	/* Update except CSE RW signature */
	if (cse_copy_rw(target_region, cse_cbfs_rw_wo_sign, CSE_RW_SIGN_SIZE,
			cse_cbfs_rw_wo_sign_sz) != CB_SUCCESS)
		return CB_ERR;

	/* Update CSE RW signature to indicate update is complete */
	if (cse_copy_rw(target_region, (void *)cse_cbfs_rw, 0, CSE_RW_SIGN_SIZE) != CB_SUCCESS)
		return CB_ERR;

	printk(BIOS_INFO, "cse_lite: CSE RW Update Successful\n");
	return CB_SUCCESS;
}

static bool is_cse_fw_update_enabled(void)
{
	if (!CONFIG(SOC_INTEL_CSE_RW_UPDATE))
		return false;

	return true;
}

static enum csme_failure_reason cse_update_rw(const void *cse_cbfs_rw, const size_t cse_blob_sz,
	struct region *target_region)
{
	if (target_region->size < cse_blob_sz) {
		printk(BIOS_ERR, "RW update does not fit. CSE RW flash region size: %zx,"
			"Update blob size:%zx\n", target_region->size, cse_blob_sz);
		return CSE_LITE_SKU_LAYOUT_MISMATCH_ERROR;
	}

	if (cse_erase_rw_region(target_region) != CB_SUCCESS)
		return CSE_LITE_SKU_FW_UPDATE_ERROR;

	if (cse_write_rw_region(target_region, cse_cbfs_rw, cse_blob_sz) != CB_SUCCESS)
		return CSE_LITE_SKU_FW_UPDATE_ERROR;

	return CSE_NO_ERROR;
}

static enum cb_err cse_prep_for_rw_update(enum cse_update_status status)
{
	if (status == CSE_UPDATE_CORRUPTED)
		elog_add_event(ELOG_TYPE_PSR_DATA_LOST);
	/*
	 * To set CSE's operation mode to HMRFPO mode:
	 * 1. Ensure CSE to boot from RO(BP1)
	 * 2. Send HMRFPO_ENABLE command to CSE
	 */
	if (cse_boot_to_ro() != CB_SUCCESS)
		return CB_ERR;

	if ((status == CSE_UPDATE_DOWNGRADE) || (status == CSE_UPDATE_CORRUPTED)) {
		if (cse_data_clear_request() != CB_SUCCESS) {
			printk(BIOS_ERR, "cse_lite: CSE data clear failed!\n");
			return CB_ERR;
		}
	}

	return cse_hmrfpo_enable();
}

static enum csme_failure_reason cse_trigger_fw_update(enum cse_update_status status,
	struct region *target_region)
{
	enum csme_failure_reason rv;
	void *cse_cbfs_rw;
	size_t size;

	cse_cbfs_rw = cbfs_map(SOC_INTEL_CSE_RW_CBFS_NAME, &size);

	if (!cse_cbfs_rw) {
		printk(BIOS_ERR, "cse_lite: CSE CBFS RW blob could not be mapped\n");
		return CSE_LITE_SKU_RW_BLOB_NOT_FOUND;
	}

	if (cse_prep_for_rw_update(status) != CB_SUCCESS) {
		rv = CSE_COMMUNICATION_ERROR;
		goto error_exit;
	}

	cse_fw_update_misc_oper();
	rv = cse_update_rw(cse_cbfs_rw, size, target_region);

error_exit:
	cbfs_unmap(cse_cbfs_rw);
	return rv;
}

/*
 * Check if a CSE Firmware update is required
 * returns true if an update is required, false otherwise
 */
bool is_cse_fw_update_required(void)
{
	struct fw_version cbfs_rw_version;

	if (!is_cse_fw_update_enabled())
		return false;

	/*
	 * First, check if cse_bp_info_rsp global structure is populated.
	 * If not, it implies that cse_fill_bp_info() function is not called.
	 */
	if (!is_cse_bp_info_valid(&cse_bp_info_rsp))
		return false;

	if (get_cse_ver_from_cbfs(&cbfs_rw_version) == CB_ERR)
		return false;

	return !!cse_compare_sub_part_version(&cbfs_rw_version, cse_get_rw_version());
}

static uint8_t cse_fw_update(void)
{
	struct region target_region;
	enum cse_update_status status;

	if (cse_get_target_region(&target_region) != CB_SUCCESS) {
		printk(BIOS_ERR, "cse_lite: Failed to get CSE RW Partition\n");
		return CSE_LITE_SKU_RW_ACCESS_ERROR;
	}

	status = cse_check_update_status(&target_region);
	if (status == CSE_UPDATE_NOT_REQUIRED)
		return CSE_NO_ERROR;
	if (status == CSE_UPDATE_METADATA_ERROR)
		return CSE_LITE_SKU_RW_METADATA_NOT_FOUND;

	printk(BIOS_DEBUG, "cse_lite: CSE RW update is initiated\n");
	return cse_trigger_fw_update(status, &target_region);
}

static const char *cse_sub_part_str(enum bpdt_entry_type type)
{
	switch (type) {
	case IOM_FW:
		return "IOM";
	case NPHY_FW:
		return "NPHY";
	default:
		return "Unknown";
	}
}

static enum cb_err cse_locate_area_as_region_rw(size_t bp, struct region  *target_region)
{
	struct region cse_region_all;
	uint32_t size;
	uint32_t start_offset;
	uint32_t end_offset;

	if (cse_get_rw_region(&cse_region_all) != CB_SUCCESS)
		return CB_ERR;

	if (!strcmp(cse_regions[bp], "RO"))
		cse_get_bp_entry_range(RO, &start_offset, &end_offset);
	else
		cse_get_bp_entry_range(RW, &start_offset, &end_offset);

	size = end_offset + 1 - start_offset;

	struct region sub_region = {.offset = start_offset, .size = size};
	if(cse_get_effective_region(&cse_region_all, &sub_region, target_region))
		return CB_ERR;

	printk(BIOS_DEBUG, "cse_lite: CSE %s  partition: offset = 0x%x, size = 0x%x\n",
			cse_regions[bp], start_offset, size);
	return CB_SUCCESS;
}

static enum cb_err cse_sub_part_get_target_region(struct region *target_region, size_t bp,
	enum bpdt_entry_type type)
{
	struct bpdt_header bpdt_hdr;
	struct region cse_region;
	struct bpdt_entry bpdt_entries[MAX_SUBPARTS];
	uint8_t i;

	if (cse_locate_area_as_region_rw(bp, &cse_region) != CB_SUCCESS) {
		printk(BIOS_ERR, "cse_lite: Failed to locate %s in the CSE Region\n",
				cse_regions[bp]);
		return CB_ERR;
	}

	if ((cse_region_readat(&cse_region, &bpdt_hdr, 0, BPDT_HEADER_SZ)) != BPDT_HEADER_SZ) {
		printk(BIOS_ERR, "cse_lite: Failed to read BPDT header from CSE region\n");
		return CB_ERR;
	}

	if ((cse_region_readat(&cse_region, bpdt_entries, BPDT_HEADER_SZ,
		(bpdt_hdr.descriptor_count * BPDT_ENTRY_SZ))) !=
		(bpdt_hdr.descriptor_count * BPDT_ENTRY_SZ)) {
		printk(BIOS_ERR, "cse_lite: Failed to read BPDT entries from CSE region\n");
		return CB_ERR;
	}

	/* walk through BPDT entries to identify sub-partition's payload offset and size */
	for (i = 0; i < bpdt_hdr.descriptor_count; i++) {
		if (bpdt_entries[i].type == type) {
			printk(BIOS_INFO, "cse_lite: Sub-partition %s- offset = 0x%x,"
				"size = 0x%x\n", cse_sub_part_str(type), bpdt_entries[i].offset,
				bpdt_entries[i].size);
			struct region sub_region = {.offset = bpdt_entries[i].offset,
				.size = bpdt_entries[i].size};
			if(cse_get_effective_region(&cse_region, &sub_region, target_region))
				return CB_ERR;
			else
				return CB_SUCCESS;
		}
	}

	printk(BIOS_ERR, "cse_lite: Sub-partition %s is not found\n", cse_sub_part_str(type));
	return CB_ERR;
}

static enum cb_err cse_get_sub_part_fw_version(enum bpdt_entry_type type,
	const struct region *target_region, struct fw_version *fw_ver)
{
	struct subpart_entry subpart_entry;
	struct subpart_entry_manifest_header man_hdr;

	if ((cse_region_readat(target_region, &subpart_entry, SUBPART_HEADER_SZ, SUBPART_ENTRY_SZ))
			!= SUBPART_ENTRY_SZ) {
		printk(BIOS_ERR, "cse_lite: Failed to read %s sub partition entry\n",
				cse_sub_part_str(type));
		return CB_ERR;
	}

	if ((cse_region_readat(target_region, &man_hdr, subpart_entry.offset_bytes, 
			SUBPART_MANIFEST_HDR_SZ)) != SUBPART_MANIFEST_HDR_SZ) {
		printk(BIOS_ERR, "cse_lite: Failed to read %s Sub part entry #0 manifest\n",
				cse_sub_part_str(type));
		return CB_ERR;
	}

	fw_ver->major = man_hdr.binary_version.major;
	fw_ver->minor = man_hdr.binary_version.minor;
	fw_ver->hotfix = man_hdr.binary_version.hotfix;
	fw_ver->build = man_hdr.binary_version.build;

	return CB_SUCCESS;
}

static void cse_sub_part_get_source_fw_version(void *subpart_cbfs_rw, struct fw_version *fw_ver)
{
	uint8_t *ptr = (uint8_t *)subpart_cbfs_rw;
	struct subpart_entry *subpart_entry;
	struct subpart_entry_manifest_header *man_hdr;

	subpart_entry = (struct subpart_entry *)(ptr + SUBPART_HEADER_SZ);
	man_hdr = (struct subpart_entry_manifest_header *)(ptr + subpart_entry->offset_bytes);

	fw_ver->major = man_hdr->binary_version.major;
	fw_ver->minor = man_hdr->binary_version.minor;
	fw_ver->hotfix = man_hdr->binary_version.hotfix;
	fw_ver->build = man_hdr->binary_version.build;
}

static enum cb_err cse_prep_for_component_update(void)
{
	/*
	 * To set CSE's operation mode to HMRFPO mode:
	 * 1. Ensure CSE to boot from RO(BP1)
	 * 2. Send HMRFPO_ENABLE command to CSE
	 */
	if (cse_boot_to_ro() != CB_SUCCESS)
		return CB_ERR;

	return cse_hmrfpo_enable();
}

static enum csme_failure_reason cse_sub_part_trigger_update(enum bpdt_entry_type type,
	uint8_t bp, const void *subpart_cbfs_rw, const size_t blob_sz, struct region *target_region)
{
	if (target_region->size < blob_sz) {
		printk(BIOS_ERR, "cse_lite: %s Target sub-partition size: %zx, "
				"smaller than blob size:%zx, abort update\n",
				cse_sub_part_str(type), target_region->size, blob_sz);
		return CSE_LITE_SKU_SUB_PART_LAYOUT_MISMATCH_ERROR;
	}

	/* Erase CSE Lite sub-partition */
	if (cse_erase_rw_region(target_region) != CB_SUCCESS)
		return CSE_LITE_SKU_SUB_PART_UPDATE_FAIL;

	/* Update CSE Lite sub-partition */
	if (cse_copy_rw(target_region, (void *)subpart_cbfs_rw, 0, blob_sz) != CB_SUCCESS)
		return CSE_LITE_SKU_SUB_PART_UPDATE_FAIL;

	printk(BIOS_INFO, "cse_lite: CSE %s %s Update successful\n", GET_BP_STR(bp),
			cse_sub_part_str(type));

	return CSE_LITE_SKU_PART_UPDATE_SUCCESS;
}

static enum csme_failure_reason handle_cse_sub_part_fw_update_rv(enum csme_failure_reason rv)
{
	switch (rv) {
	case CSE_LITE_SKU_PART_UPDATE_SUCCESS:
	case CSE_LITE_SKU_SUB_PART_UPDATE_NOT_REQ:
		return rv;
	default:
		cse_trigger_vboot_recovery(rv);
	}
	/* Control never reaches here */
	return rv;
}

static enum csme_failure_reason cse_sub_part_fw_component_update(enum bpdt_entry_type type,
	const char *name)
{
	struct region target_region;
	struct fw_version target_fw_ver, source_fw_ver;
	enum csme_failure_reason rv;
	size_t size;

	void *subpart_cbfs_rw = cbfs_map(name, &size);
	if (!subpart_cbfs_rw) {
		printk(BIOS_ERR, "cse_lite: Not able to map %s CBFS file\n",
				cse_sub_part_str(type));
		return CSE_LITE_SKU_SUB_PART_BLOB_ACCESS_ERR;
	}

	cse_sub_part_get_source_fw_version(subpart_cbfs_rw, &source_fw_ver);
	printk(BIOS_INFO, "cse_lite: CBFS %s FW Version: %x.%x.%x.%x\n", cse_sub_part_str(type),
			source_fw_ver.major, source_fw_ver.minor, source_fw_ver.hotfix,
			source_fw_ver.build);

	/* Trigger sub-partition update in CSE RO and CSE RW */
	for (size_t bp = 0; bp < ARRAY_SIZE(cse_regions); bp++) {
		if (cse_sub_part_get_target_region(&target_region, bp, type) != CB_SUCCESS) {
			rv = CSE_LITE_SKU_SUB_PART_ACCESS_ERR;
			goto error_exit;
		}

		if (cse_get_sub_part_fw_version(type, &target_region, &target_fw_ver) != CB_SUCCESS) {
			rv = CSE_LITE_SKU_SUB_PART_ACCESS_ERR;
			goto error_exit;
		}

		printk(BIOS_INFO, "cse_lite: %s %s FW Version: %x.%x.%x.%x\n", cse_regions[bp],
				cse_sub_part_str(type), target_fw_ver.major,
				target_fw_ver.minor, target_fw_ver.hotfix, target_fw_ver.build);

		if (!cse_compare_sub_part_version(&target_fw_ver, &source_fw_ver)) {
			printk(BIOS_INFO, "cse_lite: %s %s update is not required\n",
					cse_regions[bp], cse_sub_part_str(type));
			rv = CSE_LITE_SKU_SUB_PART_UPDATE_NOT_REQ;
			continue;
		}

		printk(BIOS_INFO, "CSE %s %s Update initiated\n", GET_BP_STR(bp),
				cse_sub_part_str(type));

		if (cse_prep_for_component_update() != CB_SUCCESS) {
			rv = CSE_LITE_SKU_SUB_PART_ACCESS_ERR;
			goto error_exit;
		}

		rv = cse_sub_part_trigger_update(type, bp, subpart_cbfs_rw,
				size, &target_region);

		if (rv != CSE_LITE_SKU_PART_UPDATE_SUCCESS)
			goto error_exit;
	}
error_exit:
	cbfs_unmap(subpart_cbfs_rw);
	return rv;
}

static enum csme_failure_reason cse_sub_part_fw_update(void)
{
	if (skip_cse_sub_part_update()) {
		printk(BIOS_INFO, "CSE Sub-partition update not required\n");
		return CSE_LITE_SKU_SUB_PART_UPDATE_NOT_REQ;
	}

	enum csme_failure_reason rv;
	rv = cse_sub_part_fw_component_update(IOM_FW, SOC_INTEL_CSE_IOM_CBFS_NAME);

	handle_cse_sub_part_fw_update_rv(rv);

	rv = cse_sub_part_fw_component_update(NPHY_FW, SOC_INTEL_CSE_NPHY_CBFS_NAME);

	return handle_cse_sub_part_fw_update_rv(rv);
}

static void do_cse_fw_sync(void)
{
	/*
	 * If system is in recovery mode, skip CSE Lite update if CSE sub-partition update
	 * is not enabled and continue to update CSE sub-partitions.
	 */
	if (vboot_in_recovery()) {
		printk(BIOS_DEBUG, "cse_lite: Skip switching to RW in the recovery path\n");
		return;
	}

	/* If CSE SKU type is not Lite, skip enabling CSE Lite SKU */
	if (!cse_is_hfs3_fw_sku_lite()) {
		printk(BIOS_ERR, "cse_lite: Not a CSE Lite SKU\n");
		return;
	}

	if (cse_get_bp_info() != CB_SUCCESS) {
		printk(BIOS_ERR, "cse_lite: Failed to get CSE boot partition info\n");

		 /* If system is in recovery mode, don't trigger recovery again */
		if (!vboot_in_recovery()) {
			cse_trigger_vboot_recovery(CSE_COMMUNICATION_ERROR);
		} else {
			printk(BIOS_ERR, "cse_lite: System is already in Recovery Mode, "
					"so no action\n");
			return;
		}
	}

	/*
	 * If system is in recovery mode, CSE Lite update has to be skipped but CSE
	 * sub-partitions like NPHY and IOM have to be updated. If CSE sub-partition update
	 * fails during recovery, just continue to boot.
	 */
	if (CONFIG(SOC_INTEL_CSE_SUB_PART_UPDATE) && vboot_in_recovery()) {
		if (cse_sub_part_fw_update() == CSE_LITE_SKU_PART_UPDATE_SUCCESS) {
			do_global_reset();
			die("ERROR: GLOBAL RESET Failed to reset the system\n");
		}

		return;
	}

	if (cse_fix_data_failure_err() != CB_SUCCESS)
		cse_trigger_vboot_recovery(CSE_LITE_SKU_DATA_WIPE_ERROR);

	/*
	 * CSE firmware update is skipped if SOC_INTEL_CSE_RW_UPDATE is not defined and
	 * runtime debug control flag is not enabled. The driver triggers recovery if CSE CBFS
	 * RW metadata or CSE CBFS RW blob is not available.
	 */
	if (is_cse_fw_update_enabled()) {
		uint8_t rv;
		rv = cse_fw_update();
		if (rv)
			cse_trigger_vboot_recovery(rv);
	}

	if (CONFIG(SOC_INTEL_CSE_SUB_PART_UPDATE))
		cse_sub_part_fw_update();

	if (!cse_is_rw_bp_status_valid())
		cse_trigger_vboot_recovery(CSE_LITE_SKU_RW_JUMP_ERROR);

	if (cse_boot_to_rw() != CB_SUCCESS) {
		printk(BIOS_ERR, "cse_lite: Failed to switch to RW\n");
		cse_trigger_vboot_recovery(CSE_LITE_SKU_RW_SWITCH_ERROR);
	}

}

void cse_fw_sync(void)
{
	timestamp_add_now(TS_CSE_FW_SYNC_START);
	do_cse_fw_sync();
	timestamp_add_now(TS_CSE_FW_SYNC_END);
}

static void payload_cse_misc_ops(void *unused)
{
	if (acpi_get_sleep_type_s3())
		return;

	cse_fw_sync();
}

static int do_run_payload_cse_misc_ops(void)
{
	payload_cse_misc_ops(NULL);
	return 0;
}

INIT_FUNC(do_run_payload_cse_misc_ops);
