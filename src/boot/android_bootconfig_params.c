// SPDX-License-Identifier: GPL-2.0

#include <inttypes.h>
#include <libpayload.h>
#include <lp_vboot.h>
#include <vb2_api.h>

#include "base/cleanup_funcs.h"
#include "base/gpt.h"
#include "base/timestamp.h"
#include "base/vpd_util.h"
#include "boot/android_bootconfig_params.h"
#include "boot/android_dtboimg.h"
#include "boot/bootconfig.h"
#include "drivers/storage/blockdev.h"
#include "vboot/firmware_id.h"

#define MAX_VPD_BUFFER_SIZE 1024U

static int append_vpd(struct bootconfig *bc, const char *vpd_key,
		      const char *config_key) {
	char buffer[MAX_VPD_BUFFER_SIZE];

	if (!vpd_gets(vpd_key, buffer, sizeof(buffer))) {
		printf("%s not found in VPD\n", vpd_key);
		return -1;
	}
	return bootconfig_append(bc, config_key, buffer);
}

#define HW_DESCR_VPD_KEY "mfg_sku_id"
#define HW_DESCR_CONFIG_KEY "androidboot.product.hardware.id"

static int append_hw_descr(struct bootconfig *bc)
{
	char hwid[VB2_GBB_HWID_MAX_SIZE];
	uint32_t hwid_size = sizeof(hwid);

	if (!vb2api_gbb_read_hwid(vboot_get_context(), hwid, &hwid_size))
		return bootconfig_append(bc, HW_DESCR_CONFIG_KEY, hwid);

	return append_vpd(bc, HW_DESCR_VPD_KEY, HW_DESCR_CONFIG_KEY);
}

#define DISPLAY_ORIENTATION_KEY_STR "androidboot.surface_flinger.primary_display_orientation"
#define MAX_DISPLAY_ORIENTATION_LENGTH sizeof("ORIENTATION_xxx")

static int append_display_orientation(struct bootconfig *bc, struct vb2_kernel_params *kp)
{
	static const char orientation_map[][MAX_DISPLAY_ORIENTATION_LENGTH] = {
		[CB_FB_ORIENTATION_NORMAL] = "ORIENTATION_0",
		[CB_FB_ORIENTATION_BOTTOM_UP] = "ORIENTATION_180",
		[CB_FB_ORIENTATION_LEFT_UP] = "ORIENTATION_270",
		[CB_FB_ORIENTATION_RIGHT_UP] = "ORIENTATION_90",
	};
	uint8_t orientation = lib_sysinfo.framebuffer.orientation;

	if (orientation >= ARRAY_SIZE(orientation_map)) {
		printf("%s: Unexpected display orientation: %d\n", __func__, orientation);
		return -1;
	}
	return bootconfig_append(bc, DISPLAY_ORIENTATION_KEY_STR,
				 orientation_map[orientation]);
}

#define SKU_ID_KEY_STR "androidboot.product.vendor.sku"
/* 20 characters for model name. Suffix with 12 characters for SKU ID */
#define MAX_SKU_ID_LENGTH 32

static int append_skuid(struct bootconfig *bc)
{
	char sku_id_str[MAX_SKU_ID_LENGTH];
	uint32_t sku_id;
	struct cb_mainboard *mainboard =
		phys_to_virt(lib_sysinfo.cb_mainboard);
	const char *mb_part_string = cb_mb_part_string(mainboard);

	sku_id = lib_sysinfo.sku_id;
	int len = snprintf(sku_id_str, sizeof(sku_id_str), "%s_%u", mb_part_string, sku_id);
	if (len < 0 || len >= sizeof(sku_id_str))
		return -1;

	for (int i = 0; sku_id_str[i] != '\0'; i++)
		sku_id_str[i] = tolower(sku_id_str[i]);

	return bootconfig_append(bc, SKU_ID_KEY_STR, sku_id_str);
}

#define BOOT_PART_UUID_KEY_STR "androidboot.boot_part_uuid"

static int append_boot_part_uuid(struct bootconfig *bc, struct vb2_kernel_params *kp)
{
	char guid_str[GUID_STRLEN];
	const Guid *guid = &kp->partition_guid;

	GptGuidToStr(guid, guid_str, ARRAY_SIZE(guid_str), GPT_GUID_LOWERCASE);
	return bootconfig_append(bc, BOOT_PART_UUID_KEY_STR, guid_str);
}

#define BOOT_SOURCE_KEY_STR "androidboot.boot_source"
#define BOOT_SOURCE_VALUE_EXTERNAL_STR "external"
#define BOOT_SOURCE_VALUE_INTERNAL_STR "internal"

static int append_boot_source(struct bootconfig *bc, struct vb2_kernel_params *kp)
{
	BlockDev *bdev = (BlockDev *)kp->disk_handle;

	return bootconfig_append(bc, BOOT_SOURCE_KEY_STR,
				 bdev->removable ?
				 BOOT_SOURCE_VALUE_EXTERNAL_STR :
				 BOOT_SOURCE_VALUE_INTERNAL_STR);
}

#define BOOTLOADER_VERSION_KEY_STR "androidboot.bootloader"
#define BOOTLOADER_VERSION_PATTERN "depthcharge-%s/%s"
/* FWID is 256 bytes - to be safe, double the size of 2 FWIDs */
#define MAX_BOOTLOADER_VERSION_LENGTH 1024

static int append_bootloader_version(struct bootconfig *bc)
{
	char version_str[MAX_BOOTLOADER_VERSION_LENGTH];
	const char *fwid;
	const char *ro_fwid;
	int ret;

	fwid = get_active_fw_id();
	ro_fwid = get_ro_fw_id();

	ret = snprintf(version_str, sizeof(version_str),
		       BOOTLOADER_VERSION_PATTERN, ro_fwid, fwid);
	if (ret <= 0 || ret >= sizeof(version_str)) {
		printf("Failed to snprintf the message, ret:%d\n", ret);
		return ret;
	}

	return bootconfig_append(bc, BOOTLOADER_VERSION_KEY_STR, version_str);
}

#define DDR_SIZE_KEY_STR "androidboot.ddr_size"
#define DDR_SIZE_PATTERN "%zuGB"
#define DDR_SIZE_VALUE_LENGTH 32

static int append_ddr_size(struct bootconfig *bc)
{
	char ddr_size_str[DDR_SIZE_VALUE_LENGTH];
	const size_t memory_size_mib = sysinfo_get_physical_memory_size_mib();

	if (!memory_size_mib) {
		printf("ERROR: Unable to get physical memory size from sysinfo\n");
		return -1;
	} else if (memory_size_mib % (GiB / MiB) != 0) {
		printf("WARNING: Reported physical memory size is not aligned to GiB. Will be "
		       "rounded down. Reported value: %zu MiB (%#zx)\n",
		       memory_size_mib, memory_size_mib);
	}

	/* Change MiB to GiB. */
	int ret = snprintf(ddr_size_str, sizeof(ddr_size_str), DDR_SIZE_PATTERN,
			   memory_size_mib / (GiB / MiB));
	if (ret <= 0 || ret >= sizeof(ddr_size_str))
		return -1;

	return bootconfig_append(bc, DDR_SIZE_KEY_STR, ddr_size_str);
}

#define MFG_SKU_ID_VPD_KEY "mfg_sku_id"
#define MFG_SKU_ID_CONFIG_KEY "androidboot.product.hardware.sku"

#define SERIAL_NUM_VPD_KEY "serial_number"
#define SERIAL_NUM_CONFIG_KEY "androidboot.serialno"

#define DTBO_IDX_CONFIG_KEY "androidboot.dtbo_idx"

static int append_dtbo_indices(struct bootconfig *bc)
{
	if (CONFIG(ARCH_ARM)) {
		const char *dtbo_indices = android_get_dtbo_indices();

		if (dtbo_indices)
			return bootconfig_append(bc, DTBO_IDX_CONFIG_KEY, dtbo_indices);

		return -1;
	}
	return 0;
}

int append_android_bootconfig_params(struct bootconfig *bc, struct vb2_kernel_params *kp)
{
	return append_boot_part_uuid(bc, kp) |
	       append_hw_descr(bc) |
	       append_vpd(bc, MFG_SKU_ID_VPD_KEY, MFG_SKU_ID_CONFIG_KEY) |
	       append_vpd(bc, SERIAL_NUM_VPD_KEY, SERIAL_NUM_CONFIG_KEY) |
	       append_display_orientation(bc, kp) |
	       append_skuid(bc) |
	       append_boot_source(bc, kp) |
	       append_bootloader_version(bc) |
	       append_ddr_size(bc) |
	       append_dtbo_indices(bc);
}

int append_android_bootconfig_boottime(struct boot_info *bi)
{
	struct bootconfig bc;
	struct bootconfig_trailer *bc_trailer;

	if (!bi->ramdisk_size)
		return -1;

	bc_trailer = (struct bootconfig_trailer *)((uintptr_t)bi->ramdisk_addr +
		      bi->ramdisk_size - sizeof(*bc_trailer));

	if (bootconfig_reinit(&bc, bc_trailer))
		return -1;

	/* Append current boottime */
	uint64_t boot_time_ms = get_us_since_pre_cpu_reset() / USECS_PER_MSEC;
	uint64_t splash_time_ms = get_us_timestamp_since_pre_reset_at_index(
				TS_FIRMWARE_SPLASH_RENDERED) / USECS_PER_MSEC;
	char boottime[sizeof(BOOTCONFIG_MAX_BOOTTIME_STR)];
	snprintf(boottime, sizeof(boottime), "firmware:%"PRIu64",splash:%"PRIu64,
	       boot_time_ms, splash_time_ms);
	if (bootconfig_append(&bc, BOOTCONFIG_BOOTTIME_KEY_STR, boottime)) {
		printf("%s: Cannot append boottime", __func__);
		return -1;
	}
	/* Recalculate bootconfig checksum after changes */
	bootconfig_checksum_recalculate(&bc, bc_trailer);

	return 0;
}
