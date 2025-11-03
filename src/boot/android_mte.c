// SPDX-License-Identifier: GPL-2.0

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "base/android_misc.h"
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

static bool misc_ctrl_inited;
static struct mte_ctrl misc_ctrl;

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

static void apply_misc_mte_ctrl(struct mte_ctrl *ctrl)
{
	if (!misc_ctrl_inited) {
		printf("%s: No valid MTE control from misc partition\n", __func__);
		return;
	}

	ctrl->mte = misc_ctrl.mte || misc_ctrl.mte_once;
	ctrl->kasan = misc_ctrl.kasan || misc_ctrl.kasan_once;

	if (misc_ctrl.kasan_mode != KASAN_MODE_NOT_SET)
		ctrl->kasan_mode = misc_ctrl.kasan_mode;
	if (misc_ctrl.kasan_fault != KASAN_FAULT_NOT_SET)
		ctrl->kasan_fault = misc_ctrl.kasan_fault;
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

	apply_misc_mte_ctrl(&ctrl);

	return append_mte_params_to_cmdline(cmdline_buf, size, &ctrl);
}

void android_mte_get_misc_ctrl(BlockDev *bdev, GptData *gpt)
{
	struct misc_system_space space;
	struct misc_memtag_message *msg;
	uint32_t memtag_mode;

	misc_ctrl_inited = false;

	if (android_misc_system_space_read(bdev, gpt, &space))
		return;

	msg = &space.memtag_message;

	if (!msg->version || msg->magic != ANDROID_MISC_MEMTAG_MAGIC) {
		printf("%s: Invalid memtag message data\n", __func__);
		return;
	}

	memtag_mode = msg->memtag_mode;
	misc_ctrl.raw = (uint16_t)msg->memtag_mode;
	printf("%s: memtag_mode %#x\n", __func__, memtag_mode);

	/*
	 * According to https://source.android.com/docs/security/test/memory-safety/bootloader-support,
	 * bootloader MUST clear MISC_MEMTAG_MODE_MEMTAG_ONCE and
	 * MISC_MEMTAG_MODE_MEMTAG_KERNEL_ONCE on every boot.
	 * */
	memtag_mode &= ~(ANDROID_MISC_MEMTAG_MODE_MEMTAG_ONCE |
			 ANDROID_MISC_MEMTAG_MODE_MEMTAG_KERNEL_ONCE);

	if (memtag_mode != msg->memtag_mode) {
		msg->memtag_mode = memtag_mode;
		android_misc_system_space_write(bdev, gpt, &space);
	}
	misc_ctrl_inited = true;
}
