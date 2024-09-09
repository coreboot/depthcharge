// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>

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

static int append_serial_num(struct bootconfig *bc)
{
	char serial_num[MAX_SERIAL_NUM_LENGTH];

	if (!vpd_gets("serial_number", serial_num, sizeof(serial_num))) {
		printf("No serial number in vpd\n");
		return -1;
	}
	return bootconfig_append_params(bc, SERIAL_NUM_KEY_STR, serial_num);
}

static int append_boottime(struct bootconfig *bc)
{
	/* Need to record boot time in ms. This is the requirement on bootloader from utilities
	   like bootanalyze.py and aligns with other Android platforms. */
	uint64_t boot_time_ms = get_us_since_pre_cpu_reset() / USECS_PER_MSEC;
	char boottime[MAX_BOOTTIME_LENGTH];

	/* Reserve 20 digits to record maximum value of uint64_t (18446744073709551615). */
	snprintf(boottime, sizeof(boottime), "firmware:%-20lld", boot_time_ms);
	return bootconfig_append_params(bc, BOOTTIME_KEY_STR, boottime);
}

enum bootconfig_param_index {
	SERIAL_NUM,
	BOOTTIME,
};

static struct {
	const char *name;
	int (*const append)(struct bootconfig *bc);
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

int append_android_bootconfig_params(struct bootconfig *bc)
{
	int ret;

	for (unsigned int i = 0; i < ARRAY_SIZE(params); i++) {
		ret = params[i].append(bc);
		if (ret < 0) {
			printf("Cannot append %s to Android bootconfig!\n", params[i].name);
			continue;
		}
	}
	return 0;
}
