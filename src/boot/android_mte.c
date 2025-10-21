// SPDX-License-Identifier: GPL-2.0

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "boot/android_mte.h"
#include "boot/commandline.h"

#define MTE_CMDLINE_CTRL	"bl.mtectrl"

enum {
	KASAN_MODE_NOT_SET = 0,
	KASAN_MODE_ASYNC,
	KASAN_MODE_ASYMM,
	KASAN_MODE_SYNC,
};

enum {
	KASAN_FAULT_NOT_SET = 0,
	KASAN_FAULT_REPORT,
	KASAN_FAULT_PANIC_ON_WRITE,
	KASAN_FAULT_PANIC,
};

/*
 * Refer to the below URL for bitfield definition.
 * https://googleplex-android.googlesource.com/platform/bootable/recovery/+/refs/heads/main/bootloader_message/include/bootloader_message/bootloader_message_core.h
 */
struct mte_ctrl {
	union {
		struct {
			uint16_t mte:1;
			uint16_t mte_once:1;
			uint16_t kasan:1;
			uint16_t kasan_once:1;
			uint16_t mte_off:1;
			uint16_t forced:1;
			uint16_t rsvd_6_7:2;
			uint16_t kasan_mode:2;
			uint16_t rsvd_10_11:2;
			uint16_t kasan_fault:2;
			uint16_t rsvd_14_15:2;
		};
		uint16_t raw;
	};
};

static int append_mte_params_to_cmdline(char *cmdline_buf, size_t size,
					const struct mte_ctrl *ctrl)
{
	size_t initial_len = strlen(cmdline_buf);
	char *curr = cmdline_buf + initial_len;
	char *end = cmdline_buf + size;

#define APPEND_CMDLINE(fmt, ...) \
	do { \
		int ret = snprintf(curr, end - curr, fmt, ##__VA_ARGS__); \
		if (ret < 0 || (size_t)ret >= (end - curr)) { \
			printf("ERROR: Insufficient buffer for MTE command line (%s).\n", \
			       fmt); \
			return -1; \
		} \
		curr += ret; \
	} while (0)

	printf("MTE control value: %#x\n", ctrl->raw);

	if (!ctrl->mte) {
		APPEND_CMDLINE(" %s", "arm64.nomte");
	} else {
		APPEND_CMDLINE(" kasan=%s", ctrl->kasan ? "on" : "off");

		const char *const kasan_modes[] = {
			[KASAN_MODE_ASYNC] = "async",
			[KASAN_MODE_ASYMM] = "asymm",
			[KASAN_MODE_SYNC] = "sync",
		};

		if (ctrl->kasan_mode != KASAN_MODE_NOT_SET &&
		    ctrl->kasan_mode < ARRAY_SIZE(kasan_modes))
			APPEND_CMDLINE(" kasan.mode=%s", kasan_modes[ctrl->kasan_mode]);

		const char *const kasan_faults[] = {
			[KASAN_FAULT_REPORT] = "report",
			[KASAN_FAULT_PANIC_ON_WRITE] = "panic_on_write",
			[KASAN_FAULT_PANIC] = "panic",
		};

		if (ctrl->kasan_fault != KASAN_FAULT_NOT_SET &&
		    ctrl->kasan_fault < ARRAY_SIZE(kasan_faults))
			APPEND_CMDLINE(" kasan.fault=%s", kasan_faults[ctrl->kasan_fault]);
	}

#undef APPEND_CMDLINE

	printf("Newly appended kernel command line parameters: %s\n",
	       cmdline_buf + initial_len);

	return 0;
}

int android_mte_setup(char *cmdline_buf, size_t size)
{
	struct mte_ctrl ctrl;
	uint32_t cmdline_value;

	if (!get_cmdline_uint32_value(cmdline_buf, MTE_CMDLINE_CTRL, &cmdline_value)) {
		printf("%s: cmdline_value %#x\n", __func__, cmdline_value);
		ctrl.raw = cmdline_value & 0xFFFF;
	} else {
		printf("%s: Cannot get cmdline %s; using 0 to disable MTE\n",
		       __func__, MTE_CMDLINE_CTRL);
		ctrl.raw = 0;
	}

	return append_mte_params_to_cmdline(cmdline_buf, size, &ctrl);
}
