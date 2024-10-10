// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <vb2_android_bootimg.h>

#include "base/init_funcs.h"
#include "base/timestamp.h"
#include "base/vpd_util.h"
#include "boot/android_bootconfig_params.h"
#include "boot/bootconfig.h"
#include "boot/commandline.h"
#include "vboot/boot_policy.h"

#define SERIAL_NUM_KEY_STR "androidboot.serialno"
#define MAX_SERIAL_NUM_LENGTH CB_MAX_SERIALNO_LENGTH

#define BOOTTIME_KEY_STR "androidboot.boottime"
/* 20 characters are sufficient for max uint64 18446744073709551615. Prepend with "firmware:" */
#define MAX_BOOTTIME_LENGTH 32

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
	/* Need to record boot time in ns as per
	   https://android.googlesource.com/platform/system/core/+/HEAD/init/README.md#boot-timing */
	uint64_t boot_time_ns = get_us_since_pre_cpu_reset() * NSECS_PER_USEC;
	char boottime[MAX_BOOTTIME_LENGTH];

	/* Reserve 20 digits to record maximum value of uint64_t (18446744073709551615). */
	snprintf(boottime, sizeof(boottime), "firmware:%-20lld", boot_time_ns);
	return append_bootconfig_params(BOOTTIME_KEY_STR, boottime,
					bootc_start, bootc_size, buf_size);
}

enum bootconfig_param_index {
	SERIAL_NUM,
	BOOTTIME,
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
};

int append_android_bootconfig_params(struct VbSelectAndLoadKernelParams *kparams, void *bootc_start)
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
