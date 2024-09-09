// SPDX-License-Identifier: GPL-2.0

#include <assert.h>
#include <libpayload.h>

#include "base/init_funcs.h"
#include "base/vpd_util.h"
#include "boot/android_kernel_cmdline.h"
#include "boot/commandline.h"
#include "vboot/boot_policy.h"

#define SERIAL_NUM_PROPERTY_NAME "androidboot.serialno="
#define SERIAL_NUM_PROPERTY_NAME_LEN strlen(SERIAL_NUM_PROPERTY_NAME)
#define MAX_SERIAL_NUM_LENGTH CB_MAX_SERIALNO_LENGTH

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

void android_kernel_cmdline_fixup(VbSelectAndLoadKernelParams *kparams)
{
	/* Fixup kernel cmdline only for Android GKI images */
	if (GET_KERNEL_IMG_TYPE(kparams->flags) != KERNEL_IMAGE_ANDROID_GKI)
		return;

	append_serial_num_to_android_kernel_cmdline();
}
