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

#include "net/uip.h"
#include "fastboot/cmd.h"
#include "fastboot/disk.h"
#include "fastboot/fastboot.h"
#include "fastboot/vars.h"
#include "gpt_misc.h"
#include <stdlib.h>

static int parse_hex(const char *str, uint32_t *ret)
{
	char c;
	int valid = 0;
	uint32_t result = 0;

	while ((c = *str++)) {
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

static void fastboot_cmd_continue(struct FastbootOps *fb, const char *arg)
{
	fastboot_okay(fb, "Continuing boot");
	fb->state = FINISHED;
}

static void fastboot_cmd_upload(struct FastbootOps *fb, const char *arg)
{
	uint64_t len;
	void *buf = fastboot_get_memory_buffer(fb, &len);

	if (!fb->has_staged_data || len == 0) {
		fastboot_fail(fb, "No staged data to upload");
		return;
	}

	/* TODO(b/430379190): Implement packet splitting for fastboot tcp */
	if (len > CONFIG_UIP_TCP_MSS) {
		fastboot_info(fb,
			      "b/430379190: max upload bytes is %lld, got %lld",
			      (uint64_t)CONFIG_UIP_TCP_MSS, len);
		fastboot_fail(fb, "b/430379190: staged buffer is too large");
		return;
	}

	fastboot_data(fb, len);
	fb->send_packet(fb, buf, len);
	fastboot_succeed(fb);

	fastboot_reset_staging(fb);
}

static void fastboot_cmd_download(struct FastbootOps *fb, const char *arg)
{
	uint32_t size = 0;
	int digits = parse_hex(arg, &size);
	if (arg[digits] != '\0') {
		fastboot_fail(fb, "Invalid argument");
		return;
	}

	if (size > FASTBOOT_MAX_DOWNLOAD_SIZE) {
		fastboot_fail(fb, "File too big");
		return;
	}

	fastboot_prepare_download(fb, size);
	fastboot_data(fb, size);
}

static void fastboot_cmd_flash(struct FastbootOps *fb, const char *arg)
{
	if (!fb->has_staged_data) {
		fastboot_fail(fb, "No data staged to flash");
		return;
	}

	struct fastboot_disk disk;
	if (!fastboot_disk_init(&disk)) {
		fastboot_fail(fb, "Failed to init disk");
		return;
	}

	uint64_t data_len;
	void *data = fastboot_get_memory_buffer(fb, &data_len);
	fastboot_write(fb, &disk, arg, data, (uint32_t)data_len);
	fastboot_disk_destroy(&disk);
}

static void fastboot_cmd_erase(struct FastbootOps *fb, const char *arg)
{
	struct fastboot_disk disk;
	if (!fastboot_disk_init(&disk)) {
		fastboot_fail(fb, "Failed to init disk");
		return;
	}

	fastboot_erase(fb, &disk, arg);
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
static void fastboot_cmd_oem_get_kernels(struct FastbootOps *fb, const char *arg)
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

static void fastboot_cmd_reboot(struct FastbootOps *fb, const char *arg)
{
	fastboot_succeed(fb);
	fb->state = REBOOT;
}

static void fastboot_cmd_set_active(struct FastbootOps *fb, const char *arg)
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
	CMD_NO_ARGS("upload", fastboot_cmd_upload),
	CMD_ARGS("download", ':', fastboot_cmd_download),
	CMD_ARGS("erase", ':', fastboot_cmd_erase),
	CMD_ARGS("flash", ':', fastboot_cmd_flash),
	CMD_ARGS("getvar", ':', fastboot_cmd_getvar),
	CMD_NO_ARGS("oem get-kernels", fastboot_cmd_oem_get_kernels),
	CMD_NO_ARGS("reboot", fastboot_cmd_reboot),
	CMD_ARGS("set_active", ':', fastboot_cmd_set_active),
	{
		.name = NULL,
	},
};
