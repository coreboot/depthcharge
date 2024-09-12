// SPDX-License-Identifier: GPL-2.0

#include <assert.h>
#include <libpayload.h>

#include "base/init_funcs.h"
#include "base/timestamp.h"
#include "base/vpd_util.h"
#include "boot/android_kernel_cmdline.h"
#include "boot/commandline.h"
#include "vboot/boot_policy.h"

#define SERIAL_NUM_PROPERTY_NAME "androidboot.serialno="
#define SERIAL_NUM_PROPERTY_NAME_LEN strlen(SERIAL_NUM_PROPERTY_NAME)
#define MAX_SERIAL_NUM_LENGTH CB_MAX_SERIALNO_LENGTH

#define BOOTTIME_PROPERTY_NAME "androidboot.boottime=firmware:"
#define BOOTTIME_PROPERTY_NAME_LEN strlen(BOOTTIME_PROPERTY_NAME)
/* 20 characters are sufficient for max uint64 18446744073709551615 */
#define MAX_BOOTTIME_LENGTH 32

static void append_serial_num_to_android_kernel_cmdline(void)
{
	char serial_num[MAX_SERIAL_NUM_LENGTH];
	char key_val[SERIAL_NUM_PROPERTY_NAME_LEN + MAX_SERIAL_NUM_LENGTH + 1];

	if (!vpd_gets("serial_number", serial_num, sizeof(serial_num))) {
		printf("No serial number in vpd\n");
		return;
	}
	snprintf(key_val, sizeof(key_val), "%s%s", SERIAL_NUM_PROPERTY_NAME, serial_num);
	commandline_append(key_val);
}

static void append_boot_time_to_android_kernel_cmdline(void)
{
	/* Need to record boot time in ns as per
	   https://android.googlesource.com/platform/system/core/+/HEAD/init/README.md#boot-timing */
	uint64_t boot_time_ns = get_us_since_pre_cpu_reset() * NSECS_PER_USEC;
	char key_val[BOOTTIME_PROPERTY_NAME_LEN + MAX_BOOTTIME_LENGTH];

	snprintf(key_val, sizeof(key_val), "%s%lld", BOOTTIME_PROPERTY_NAME, boot_time_ns);
	commandline_append(key_val);
}

void android_kernel_cmdline_fixup(VbSelectAndLoadKernelParams *kparams)
{
	/* Fixup kernel cmdline only for Android GKI images */
	if (GET_KERNEL_IMG_TYPE(kparams->flags) != KERNEL_IMAGE_ANDROID_GKI)
		return;

	append_serial_num_to_android_kernel_cmdline();
	append_boot_time_to_android_kernel_cmdline();
}
