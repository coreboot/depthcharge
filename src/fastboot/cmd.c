/*
 * Copyright 2021 Google LLC
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <stdlib.h>
#include <vb2_android_misc.h>

#include "fastboot/cmd.h"
#include "fastboot/disk.h"
#include "fastboot/fastboot.h"
#include "fastboot/vars.h"
#include "gpt_misc.h"

static int parse_hex(const char *str, size_t len, uint32_t *ret)
{
	int valid = 0;
	uint32_t result = 0;
	for (size_t i = 0; i < len; i++) {
		char c = str[i];
		int units = 0;
		if (c >= '0' && c <= '9') {
			units = c - '0';
		} else if (c >= 'A' && c <= 'F') {
			units = 10 + (c - 'A');
		} else if (c >= 'a' && c <= 'f') {
			units = 10 + (c - 'a');
		} else {
			break;
		}

		result *= 0x10;
		result += units;
		valid++;
	}

	*ret = result;

	return valid;
}

static void fastboot_cmd_continue(struct FastbootOps *fb, const char *arg,
				  uint64_t arg_len)
{
	fastboot_okay(fb, "Continuing boot");
	fb->state = FINISHED;
}

static void fastboot_cmd_download(struct FastbootOps *fb, const char *arg,
				  uint64_t arg_len)
{
	uint32_t size = 0;
	int digits = parse_hex(arg, arg_len, &size);
	if (digits != arg_len) {
		fastboot_fail(fb, "Invalid argument");
		return;
	}

	if (size > FASTBOOT_MAX_DOWNLOAD_SIZE) {
		fastboot_fail(fb, "File too big");
		return;
	}

	fastboot_data(fb, size);
}

static void fastboot_cmd_flash(struct FastbootOps *fb, const char *arg,
			       uint64_t arg_len)
{
	if (!fb->has_download) {
		fastboot_fail(fb, "No data staged to flash");
		return;
	}

	struct fastboot_disk disk;
	if (!fastboot_disk_init(&disk)) {
		fastboot_fail(fb, "Failed to init disk");
		return;
	}

	uint64_t data_len;
	void *data = fastboot_get_download_buffer(fb, &data_len);
	fastboot_write(fb, &disk, arg, arg_len, data, (uint32_t)data_len, 0);
	fastboot_disk_destroy(&disk);
}

static void fastboot_cmd_erase(struct FastbootOps *fb, const char *arg,
			       uint64_t arg_len)
{
	struct fastboot_disk disk;
	if (!fastboot_disk_init(&disk)) {
		fastboot_fail(fb, "Failed to init disk");
		return;
	}

	fastboot_erase(fb, &disk, arg, arg_len);
	fastboot_disk_destroy(&disk);
}

// `fastboot oem get-kernels` returns a list of slot letter:kernel mapping.
// This is useful for us because the partition tables are more flexible than on
// more traditional devices, so there could be many kernels.
// For instance, installing Fuchsia on a Chromebook will result in the device
// having five kernel partitions:
// KERN-A
// KERN-B
// zircon-a
// zircon-b
// zircon-r
//
// We map them to fastboot slots by having the first slot be the first partition
// we found.
struct get_kernels_ctx {
	struct FastbootOps *fb;
	int cur_kernel;
};
static bool get_kernels_cb(void *ctx, int index, GptEntry *e,
			   char *partition_name)
{
	if (get_slot_for_partition_name(e, partition_name) == 0)
		return false;

	struct get_kernels_ctx *gk = (struct get_kernels_ctx *)ctx;
	char fastboot_slot = gk->cur_kernel + 'a';
	fastboot_info(gk->fb, "%c:%s:prio=%d", fastboot_slot, partition_name,
		      GetEntryPriority(e));
	gk->cur_kernel++;
	return false;
}
static void fastboot_cmd_oem_get_kernels(struct FastbootOps *fb,
					 const char *arg, uint64_t arg_len)
{
	struct fastboot_disk disk;
	if (!fastboot_disk_init(&disk)) {
		fastboot_fail(fb, "Failed to init disk");
		return;
	}
	struct get_kernels_ctx ctx = {
		.fb = fb,
		.cur_kernel = 0,
	};
	fastboot_disk_foreach_partition(&disk, get_kernels_cb, &ctx);
	fastboot_disk_destroy(&disk);
	fastboot_succeed(fb);
}

#define BCB_RECOVERY_ARG0 "recovery"
#define BCB_RECOVERY_ARG_FASTBOOT "--fastboot"

static void fastboot_cmd_reboot_to_recovery(struct FastbootOps *fb, const char *arg,
					    uint64_t arg_len)
{
	struct fastboot_disk disk;
	struct vb2_bootloader_message bcb;

	memset(&bcb, 0, sizeof(bcb));
	strcpy(bcb.command, "boot-recovery");

	/* Setup recovery arguments depending on the target */
	if (!strncmp("fastboot", arg, arg_len)) {
		snprintf(bcb.recovery, sizeof(bcb.recovery), "%s\n%s\n", BCB_RECOVERY_ARG0,
			 BCB_RECOVERY_ARG_FASTBOOT);
	} else if (!strncmp("recovery", arg, arg_len)) {
		snprintf(bcb.recovery, sizeof(bcb.recovery), "%s\n", BCB_RECOVERY_ARG0);
	} else {
		fastboot_fail(fb, "Unknown reboot target");
		return;
	}

	if (!fastboot_disk_init(&disk)) {
		fastboot_fail(fb, "Failed to init disk");
		return;
	}

	fastboot_write(fb, &disk, GPT_ENT_NAME_ANDROID_MISC,
		       sizeof(GPT_ENT_NAME_ANDROID_MISC), &bcb, sizeof(bcb), 0);

	fastboot_disk_destroy(&disk);
	/*
	 * TODO(b/370988331): We should force boot from internal drive without rebooting
	 *                    to speed up this.
	 */
	fb->state = REBOOT;
}

static void fastboot_cmd_reboot(struct FastbootOps *fb, const char *arg,
				uint64_t arg_len)
{
	fastboot_succeed(fb);
	fb->state = REBOOT;
}

static void fastboot_cmd_set_active(struct FastbootOps *fb, const char *arg,
				    uint64_t arg_len)
{
	struct fastboot_disk disk;
	if (!fastboot_disk_init(&disk)) {
		fastboot_fail(fb, "Failed to init disk");
		return;
	}
	GptEntry *slot = fastboot_get_kernel_for_slot(&disk, arg[0]);
	if (slot == NULL) {
		fastboot_fail(fb, "Could not find slot");
		goto out;
	}

	fastboot_slots_disable_all(&disk);
	GptUpdateKernelWithEntry(disk.gpt, slot, GPT_UPDATE_ENTRY_ACTIVE);

	fastboot_succeed(fb);
out:
	fastboot_disk_destroy(&disk);
}

#define CMD_ARGS(_name, _sep, _fn)                                             \
	{                                                                      \
		.name = _name, .has_args = true, .sep = _sep, .fn = _fn        \
	}
#define CMD_NO_ARGS(_name, _fn)                                                \
	{                                                                      \
		.name = _name, .has_args = false, .fn = _fn                    \
	}
struct fastboot_cmd fastboot_cmds[] = {
	CMD_NO_ARGS("continue", fastboot_cmd_continue),
	CMD_ARGS("download", ':', fastboot_cmd_download),
	CMD_ARGS("erase", ':', fastboot_cmd_erase),
	CMD_ARGS("flash", ':', fastboot_cmd_flash),
	CMD_ARGS("getvar", ':', fastboot_cmd_getvar),
	CMD_NO_ARGS("oem get-kernels", fastboot_cmd_oem_get_kernels),
	CMD_ARGS("reboot", '-', fastboot_cmd_reboot_to_recovery),
	CMD_NO_ARGS("reboot", fastboot_cmd_reboot),
	CMD_ARGS("set_active", ':', fastboot_cmd_set_active),
	{
		.name = NULL,
	},
};
