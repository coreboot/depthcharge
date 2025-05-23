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
#include "boot/bootconfig.h"

#define HWID_KEY_STR "androidboot.product.hardware.id"

static int append_hwid(struct bootconfig *bc)
{
	char hwid[VB2_GBB_HWID_MAX_SIZE];
	uint32_t hwid_size = sizeof(hwid);

	if (vb2api_gbb_read_hwid(vboot_get_context(), hwid, &hwid_size)) {
		printf("Failed to read HWID in GBB\n");
		return -1;
	}
	return bootconfig_append(bc, HWID_KEY_STR, hwid);
}

#define SERIAL_NUM_KEY_STR "androidboot.serialno"
#define MAX_SERIAL_NUM_LENGTH CB_MAX_SERIALNO_LENGTH

static int append_serial_num(struct bootconfig *bc, struct vb2_kernel_params *kp)
{
	u32 size, offset_unused;
	const void *buffer = vpd_find("serial_number", NULL, &offset_unused, &size);
	if (!buffer) {
		printf("No serial number in vpd\n");
		return 0;
	}

	char *scratch = malloc(size + 1);
	if (!scratch)
		return -1;

	memcpy(scratch, buffer, size);
	scratch[size] = '\0';
	int ret = bootconfig_append(bc, SERIAL_NUM_KEY_STR, scratch);

	free(scratch);

	return ret;
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

#define SKU_ID_KEY_STR "androidboot.product.hardware.sku"
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
	const Guid *guid = (const Guid *)kp->partition_guid;

	GptGuidToStr(guid, guid_str, ARRAY_SIZE(guid_str), GPT_GUID_LOWERCASE);
	return bootconfig_append(bc, BOOT_PART_UUID_KEY_STR, guid_str);
}

int append_android_bootconfig_params(struct bootconfig *bc, struct vb2_kernel_params *kp)
{
	return append_hwid(bc) ||
	       append_serial_num(bc, kp) ||
	       append_display_orientation(bc, kp) ||
	       append_skuid(bc) ||
	       append_boot_part_uuid(bc, kp);
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
