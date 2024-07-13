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

#ifndef SOC_INTEL_COMMON_CSE_LITE_H
#define SOC_INTEL_COMMON_CSE_LITE_H

#include <sys/types.h>

/* Converts bp index to boot partition string */
#define GET_BP_STR(bp_index) (bp_index ? "RW" : "RO")

/* CSE RW boot partition signature */
#define CSE_RW_SIGNATURE	0x000055aa

/* CSE RW boot partition signature size */
#define CSE_RW_SIGN_SIZE	sizeof(uint32_t)

/*
 * CSE Firmware supports 3 boot partitions. For CSE Lite SKU, only 2 boot partitions are
 * used and 3rd boot partition is set to BP_STATUS_PARTITION_NOT_PRESENT.
 * CSE Lite SKU Image Layout:
 * +------------+    +----+------+----+    +-----+------+-----+
 * | CSE REGION | => | RO | DATA | RW | => | BP1 | DATA | BP2 |
 * +------------+    +----+------+----+    +-----+------+-----+
 */
#define CSE_MAX_BOOT_PARTITIONS 3

/* Name of CSE Region in FMAP */
#define SOC_INTEL_CSE_FMAP_NAME		"SI_ME"

/* CBFS name for CSE sub-partition IOM binary */
#define SOC_INTEL_CSE_IOM_CBFS_NAME		"cse_iom"

/* CBFS name for CSE sub-partition NPHY binary */
#define SOC_INTEL_CSE_NPHY_CBFS_NAME	"cse_nphy"

/* CBFS entry name for CSE RW blob */
#define SOC_INTEL_CSE_RW_CBFS_NAME		"me_rw"

/* CBFS name for CSE RW hash file */
#define SOC_INTEL_CSE_RW_HASH_CBFS_NAME		"me_rw.hash"

/* CBFS name for CSE RW version file */
#define SOC_INTEL_CSE_RW_VERSION_CBFS_NAME	"me_rw.version"

/* CSE Lite SKU's valid bootable partition identifiers */
enum boot_partition_id {
	/* RO(BP1) contains recovery/minimal boot firmware */
	RO = 0,

	/* RW(BP2) contains fully functional CSE firmware */
	RW = 1
};

/*
 * Boot partition status.
 * The status is returned in response to MKHI_BUP_COMMON_GET_BOOT_PARTITION_INFO cmd.
 */
enum bp_status {
	/* This value is returned when a partition has no errors */
	BP_STATUS_SUCCESS = 0,

	/*
	 * This value is returned when a partition should be present based on layout, but it is
	 * not valid.
	 */
	BP_STATUS_GENERAL_FAILURE = 1,

	/* This value is returned when a partition is not present per initial image layout */
	BP_STATUS_PARTITION_NOT_PRESENT = 2,

	/*
	 * This value is returned when unexpected issues are detected in CSE Data area
	 * and CSE TCB-SVN downgrade scenario.
	 */
	BP_STATUS_DATA_FAILURE = 3,
};

/*
 * Boot Partition Info Flags
 * The flags are returned in response to MKHI_BUP_COMMON_GET_BOOT_PARTITION_INFO cmd.
 */
enum bp_info_flags {
	/* Redundancy Enabled: It indicates CSE supports RO(BP1) and RW(BP2) regions */
	BP_INFO_REDUNDANCY_EN = 1 << 0,

	/* It indicates RO(BP1) supports Minimal Recovery Mode */
	BP_INFO_MIN_RECOV_MODE_EN = 1 << 1,

	/*
	 * Read-only Config Enabled: It indicates HW protection to CSE RO region is enabled.
	 * The option is relevant only if the BP_INFO_MIN_RECOV_MODE_EN flag is enabled.
	 */
	BP_INFO_READ_ONLY_CFG = 1 << 2,
};

/* CSE boot partition entry info */
struct cse_bp_entry {
	/* Boot partition version */
	struct fw_version fw_ver;

	/* Boot partition status */
	uint32_t status;

	/* Starting offset of the partition within CSE region */
	uint32_t start_offset;

	/* Ending offset of the partition within CSE region */
	uint32_t end_offset;
	uint8_t reserved[12];
} __packed;

/* CSE boot partition info */
struct cse_bp_info {
	/* Number of boot partitions */
	uint8_t total_number_of_bp;

	/* Current boot partition */
	uint8_t current_bp;

	/* Next boot partition */
	uint8_t next_bp;

	/* Boot Partition Info Flags */
	uint8_t flags;

	/* Boot Partition Entry Info */
	struct cse_bp_entry bp_entries[CSE_MAX_BOOT_PARTITIONS];
} __packed;

/* CSE firmware states */
enum cse_fw_state {
	/* The CMOS and CBMEM have the current fw version. */
	CSE_FW_WARM_BOOT,

	/* The CMOS has the current fw version, and the CBMEM is wiped out. */
	CSE_FW_COLD_BOOT,

	/* The CMOS and CBMEM are not initialized or not same as running firmware version.*/
	CSE_FW_INVALID,
};

enum cse_update_status {
	CSE_UPDATE_NOT_REQUIRED,
	CSE_UPDATE_UPGRADE,
	CSE_UPDATE_DOWNGRADE,
	CSE_UPDATE_CORRUPTED,
	CSE_UPDATE_METADATA_ERROR,
};

struct get_bp_info_rsp {
	struct mkhi_hdr hdr;
	struct cse_bp_info bp_info;
} __packed;

#endif // SOC_INTEL_COMMON_CSE_LITE_H
