// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <vb2_android_bootimg.h>
#include <vb2_api.h>

#include "base/init_funcs.h"
#include "base/timestamp.h"
#include "base/vpd_util.h"
#include "boot/android_bootconfig_params.h"
#include "boot/bootconfig.h"
#include "boot/commandline.h"
#include "vboot/boot_policy.h"
#include "vboot/util/commonparams.h"

#define SERIAL_NUM_KEY_STR "androidboot.serialno"
#define MAX_SERIAL_NUM_LENGTH CB_MAX_SERIALNO_LENGTH

#define BOOTTIME_KEY_STR "androidboot.boottime"
/* 20 characters are sufficient for max uint64 18446744073709551615. Prepend with "firmware:" */
#define MAX_BOOTTIME_LENGTH 32

#define DISPLAY_ORIENTATION_KEY_STR "androidboot.surface_flinger.primary_display_orientation"
#define MAX_DISPLAY_ORIENTATION_LENGTH sizeof("ORIENTATION_xxx")

#define HWID_KEY_STR "androidboot.product.hardware.id"

static int append_hwid(void *bootc_start, size_t bootc_size, size_t buf_size)
{
	char hwid[VB2_GBB_HWID_MAX_SIZE];
	uint32_t hwid_size = sizeof(hwid);

	if (vb2api_gbb_read_hwid(vboot_get_context(), hwid, &hwid_size)) {
		printf("No HWID in GBB\n");
		return -1;
	}
	return append_bootconfig_params(HWID_KEY_STR, hwid,
					bootc_start, bootc_size, buf_size);
}

static int append_serial_num(void *bootc_start, size_t bootc_size, size_t buf_size)
{
	char serial_num[MAX_SERIAL_NUM_LENGTH];

	if (!vpd_gets("serial_number", serial_num, sizeof(serial_num))) {
		printf("No serial number in vpd\n");
		return -1;
	}
	return append_bootconfig_params(SERIAL_NUM_KEY_STR, serial_num,
					bootc_start, bootc_size, buf_size);
}

static int append_boottime(void *bootc_start, size_t bootc_size, size_t buf_size)
{
	/* Need to record boot time in ms. This is the requirement on bootloader from utilities
	   like bootanalyze.py and aligns with other Android platforms. */
	uint64_t boot_time_ms = get_us_since_pre_cpu_reset() / USECS_PER_MSEC;
	char boottime[MAX_BOOTTIME_LENGTH];

	/* Reserve 20 digits to record maximum value of uint64_t (18446744073709551615). */
	snprintf(boottime, sizeof(boottime), "firmware:%-20lld", boot_time_ms);
	return append_bootconfig_params(BOOTTIME_KEY_STR, boottime,
					bootc_start, bootc_size, buf_size);
}

static int append_display_orientation(void *bootc_start, size_t bootc_size, size_t buf_size)
{
	char orientation_map[][MAX_DISPLAY_ORIENTATION_LENGTH] = {
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
	return append_bootconfig_params(DISPLAY_ORIENTATION_KEY_STR,
					orientation_map[orientation],
					bootc_start, bootc_size, buf_size);
}

enum bootconfig_param_index {
	SERIAL_NUM,
	BOOTTIME,
	DISPLAY_ORIENTATION,
	HWID,
};

static struct {
	const char *name;
	int (*const append)(void *bootc_start, size_t bootc_size, size_t buf_size);
	bool exists;
} params[] = {
	[SERIAL_NUM] = {
		.name = SERIAL_NUM_KEY_STR,
		.append = append_serial_num,
		.exists = false,
	},
	[BOOTTIME] = {
		.name = BOOTTIME_KEY_STR,
		.append = append_boottime,
		.exists = false,
	},
	[DISPLAY_ORIENTATION] = {
		.name = DISPLAY_ORIENTATION_KEY_STR,
		.append = append_display_orientation,
		.exists = false,
	},
	[HWID] = {
		.name = HWID_KEY_STR,
		.append = append_hwid,
		.exists = false,
	},
};

int append_android_bootconfig_params(struct vb2_kernel_params *kparams, void *bootc_start)
{
	struct vendor_boot_img_hdr_v4 *vendor_hdr;
	size_t bootc_buf_size;

	vendor_hdr = (struct vendor_boot_img_hdr_v4 *)((uintptr_t)kparams->kernel_buffer +
						       kparams->vendor_boot_offset);
	if ((uintptr_t)bootc_start > ((uintptr_t)kparams->kernel_buffer +
						       kparams->kernel_buffer_size))
		return -1;

	bootc_buf_size = (size_t)(kparams->kernel_buffer + kparams->kernel_buffer_size -
				  (uintptr_t)bootc_start);

	for (unsigned int i = 0; i < ARRAY_SIZE(params); i++) {
		int bootconfig_size = params[i].append(bootc_start,
						vendor_hdr->bootconfig_size, bootc_buf_size);
		if (bootconfig_size < 0) {
			printf("Cannot append %s to Android bootconfig!\n", params[i].name);
			continue;
		}
		vendor_hdr->bootconfig_size = bootconfig_size;
		params[i].exists = true;
	}
	return 0;
}

int fixup_android_boottime(void *ramdisk, size_t ramdisk_size,
			   size_t bootconfig_offset)
{
	void *bootc_start;
	size_t prior_bootc_size, updated_bootc_size;

	if (!params[BOOTTIME].exists) {
		/*
		 * Boottime parameter could not be added earlier either because we are booting
		 * non-Android OS or some other failure. Do not return error, just log a message
		 * and continue the bootflow.
		 */
		printf("Boottime parameter does not exist in bootconfig. No need to fix it.\n");
		return 0;
	}

	if (!ramdisk || !ramdisk_size || !bootconfig_offset ||
	    (bootconfig_offset >= ramdisk_size)) {
		printf("%s: Invalid parameters - ramdisk:%p, size:%zu, bootconfig offset:%zu\n",
			__func__, ramdisk, ramdisk_size, bootconfig_offset);
		return -1;
	}

	bootc_start = (void *)((uintptr_t)ramdisk + bootconfig_offset);
	prior_bootc_size = ramdisk_size - bootconfig_offset;

	updated_bootc_size = append_boottime(bootc_start, prior_bootc_size, prior_bootc_size);
	if (updated_bootc_size != prior_bootc_size) {
		printf("Failed to fixup boottime in Android bootconfig!\n"
		       "Updated bootconfig size(%zu) != Prior bootconfig size(%zu)\n",
		       updated_bootc_size, prior_bootc_size);
		return -1;
	}

	return 0;
}
