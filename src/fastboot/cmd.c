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
#include <vb2_gpt.h>

#include "base/android_misc.h"
#include "base/gpt.h"
#include "drivers/storage/ufs.h"
#include "fastboot/cmd.h"
#include "fastboot/disk.h"
#include "fastboot/fastboot.h"
#include "fastboot/vars.h"
#include "net/uip.h"

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

	fastboot_data(fb, (uint32_t)len);
	fb->send_packet(fb, buf, len);
	fastboot_succeed(fb);

	fastboot_reset_staging(fb);
}

static void fastboot_cmd_download(struct FastbootOps *fb, const char *arg)
{
	char *end;
	uint32_t size = strtoul(arg, &end, 16);

	if (arg == end || *end != '\0') {
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

	uint64_t data_len;
	void *data = fastboot_get_memory_buffer(fb, &data_len);
	fastboot_write(fb, arg, 0, data, (uint32_t)data_len);
}

static void fastboot_cmd_erase(struct FastbootOps *fb, const char *arg)
{
	fastboot_erase(fb, arg);
}

static bool is_cmd_quote(const char c, bool bootconfig)
{
	return c == '"' || (bootconfig && c == '\'');
}

static char *cmd_find_end(char *cmdline, bool bootconfig)
{
	char in_quote = 0;

	for (; *cmdline; cmdline++) {
		if (is_cmd_quote(*cmdline, bootconfig)) {
			/*
			 * Exit in_quote state only if ending quote match with
			 * the starting one
			 */
			if (in_quote == *cmdline)
				in_quote = 0;
			else if (!in_quote)
				in_quote = *cmdline;
			continue;
		}
		if (!in_quote && is_android_misc_cmd_separator(*cmdline, bootconfig))
			break;
	}

	return cmdline;
}

static void fastboot_cmd_cmdline_get(struct FastbootOps *fb, const char *arg,
				     bool bootconfig)
{
	struct android_misc_oem_cmdline cmd;
	char *cmdline;
	char *param;

	if (fastboot_disk_gpt_init(fb))
		return;

	if (android_misc_oem_cmdline_read(fb->disk, fb->gpt, &cmd))
		/* Failed to read cmdline, treat it as empty */
		cmdline = "\0";
	else
		cmdline = android_misc_get_oem_cmd(&cmd, bootconfig);
	if (*cmdline == '\0')
		fastboot_info(fb, "<empty>");
	while (*cmdline) {
		param = cmdline;
		cmdline = cmd_find_end(cmdline, bootconfig);
		*cmdline++ = '\0';
		while (is_android_misc_cmd_separator(*cmdline, bootconfig))
			cmdline++;
		fastboot_info(fb, "%s", param);
	}

	fastboot_succeed(fb);
}

static void fastboot_cmd_cmdline_add(struct FastbootOps *fb, const char *arg,
				     bool bootconfig)
{
	struct android_misc_oem_cmdline cmd;

	if (*arg == '\0') {
		fastboot_fail(fb, "Missing argument");
		return;
	}

	if (fastboot_disk_gpt_init(fb))
		return;

	if (android_misc_oem_cmdline_read(fb->disk, fb->gpt, &cmd)) {
		/* Let's continue with fresh cmd */
		android_misc_reset_oem_cmd(&cmd);
	}

	if (android_misc_append_oem_cmd(&cmd, arg, bootconfig)) {
		fastboot_fail(fb, "Failed to append to cmdline");
		return;
	}

	if (android_misc_oem_cmdline_write(fb->disk, fb->gpt, &cmd)) {
		fastboot_fail(fb, "Failed to write cmdline to misc");
		return;
	}

	fastboot_succeed(fb);
}

static bool is_matching(const char *param, size_t param_len, const char *pattern)
{
	const char *next_wild = pattern;
	size_t pattern_len;
	bool wild_match = false;

	if (*pattern == '*') {
		pattern++;
		/* Single '*' match with everything */
		if (*pattern == '\0')
			return true;
		/* Wild match only if '*' wasn't escaped by second '*' */
		if (*pattern != '*') {
			wild_match = true;
			next_wild = pattern;
		} else {
			/* Skip this '*' when looking for the next wild char */
			next_wild = pattern + 1;
		}
	}
	next_wild = strchr(next_wild, '*');
	if (!next_wild)
		pattern_len = strlen(pattern);
	else
		pattern_len = next_wild - pattern;

	do {
		if (param_len < pattern_len)
			return false;

		if (!strncmp(param, pattern, pattern_len)) {
			/* If this is end of pattern, we need exact match */
			if (!next_wild) {
				if (param_len == pattern_len)
					return true;
			} else {
				if (is_matching(param + pattern_len, param_len - pattern_len,
						next_wild))
					return true;
			}
		}
		/* If we are wildly matching, try at next character */
		param_len--;
		param++;
	} while (wild_match);

	return false;
}

static void fastboot_cmd_cmdline_del(struct FastbootOps *fb, const char *arg,
				     bool bootconfig)
{
	struct android_misc_oem_cmdline cmd;
	int param_len;
	char *cmdline;
	char *param_end;
	char *param;
	bool modified_cmdline = false;

	if (*arg == '\0') {
		fastboot_fail(fb, "Missing argument");
		return;
	}

	if (fastboot_disk_gpt_init(fb))
		return;

	if (android_misc_oem_cmdline_read(fb->disk, fb->gpt, &cmd)) {
		fastboot_fail(fb, "Failed to read cmdline from misc");
		return;
	}

	cmdline = android_misc_get_oem_cmd(&cmd, bootconfig);
	param_end = cmdline;
	do {
		param = param_end;
		param_end = cmd_find_end(param_end, bootconfig);
		param_len = param_end - param;
		if (is_matching(param, param_len, arg)) {
			/* Remove also the delimiter if this isn't last parameter. */
			if (*param_end != '\0')
				param_len++;
			if (android_misc_del_oem_cmd(&cmd, param - cmdline, param_len,
						     bootconfig)) {
				fastboot_fail(fb, "Failed to delete \"%.*s\" from cmdline",
					      param_len, param);
				return;
			}
			modified_cmdline = true;
			param_end = param;
			continue;
		}
		if (*param_end == '\0') {
			if (modified_cmdline &&
			    android_misc_oem_cmdline_write(fb->disk, fb->gpt, &cmd)) {
				fastboot_fail(fb, "Failed to write cmdline to misc");
				return;
			}
			fastboot_succeed(fb);
			return;
		}
		param_end++;
	} while (true);
}

static void fastboot_cmd_cmdline_set(struct FastbootOps *fb, const char *arg,
				     bool bootconfig)
{
	struct android_misc_oem_cmdline cmd;

	if (fastboot_disk_gpt_init(fb))
		return;

	if (android_misc_oem_cmdline_read(fb->disk, fb->gpt, &cmd)) {
		/* Let's continue with fresh cmd */
		android_misc_reset_oem_cmd(&cmd);
	}

	if (android_misc_set_oem_cmd(&cmd, arg, bootconfig)) {
		fastboot_fail(fb, "Failed to append to cmdline");
		return;
	}

	if (android_misc_oem_cmdline_write(fb->disk, fb->gpt, &cmd)) {
		fastboot_fail(fb, "Failed to write cmdline to misc");
		return;
	}

	fastboot_succeed(fb);
}

static void fastboot_cmd_oem_cmdline_get(struct FastbootOps *fb, const char *arg)
{
	fastboot_cmd_cmdline_get(fb, arg, false);
}

static void fastboot_cmd_oem_cmdline_add(struct FastbootOps *fb, const char *arg)
{
	fastboot_cmd_cmdline_add(fb, arg, false);
}

static void fastboot_cmd_oem_cmdline_del(struct FastbootOps *fb, const char *arg)
{
	fastboot_cmd_cmdline_del(fb, arg, false);
}

static void fastboot_cmd_oem_cmdline_set(struct FastbootOps *fb, const char *arg)
{
	fastboot_cmd_cmdline_set(fb, arg, false);
}

static void fastboot_cmd_oem_bootconfig_get(struct FastbootOps *fb, const char *arg)
{
	fastboot_cmd_cmdline_get(fb, arg, true);
}

static void fastboot_cmd_oem_bootconfig_add(struct FastbootOps *fb, const char *arg)
{
	fastboot_cmd_cmdline_add(fb, arg, true);
}

static void fastboot_cmd_oem_bootconfig_del(struct FastbootOps *fb, const char *arg)
{
	fastboot_cmd_cmdline_del(fb, arg, true);
}

static void fastboot_cmd_oem_bootconfig_set(struct FastbootOps *fb, const char *arg)
{
	fastboot_cmd_cmdline_set(fb, arg, true);
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
	struct get_kernels_ctx ctx = {
		.fb = fb,
		.cur_kernel = 0,
	};

	if (fastboot_disk_gpt_init(fb))
		return;

	gpt_foreach_partition(fb->gpt, get_kernels_cb, &ctx);
	fastboot_succeed(fb);
}

static int fastboot_parse_ufs_desc_args(struct FastbootOps *fb,
					const char *arg,
					uint32_t *idn,
					uint32_t *idx)
{
	char *arg_end;

	*idn = strtoul(arg, &arg_end, 16);
	if (arg == arg_end || *arg_end != ',') {
		fastboot_fail(fb, "Invalid argument, should be '%%x,%%x'");
		return -1;
	}
	arg = arg_end + 1;
	*idx = strtoul(arg, &arg_end, 16);
	if (arg == arg_end || *arg_end != '\0') {
		fastboot_fail(fb, "Invalid argument, should be '%%x,%%x'");
		return -1;
	}

	if (*idn > 255 || *idx > 255) {
		fastboot_fail(fb, "IDN and IDX must be in [0, 0x100] range");
		return -1;
	}

	return 0;
}

static void fastboot_cmd_oem_read_ufs_descriptor(struct FastbootOps *fb,
						 const char *arg)
{
	if (!CONFIG(DRIVER_STORAGE_UFS)) {
		fastboot_fail(fb, "UFS is not supported by the board");
		return;
	}

	UfsCtlr *ctlr = ufs_get_ctlr();

	if (!ctlr) {
		fastboot_fail(fb, "Could not find UFS controller");
		return;
	}

	uint32_t idn, idx;
	if (fastboot_parse_ufs_desc_args(fb, arg, &idn, &idx)) {
		// Parsing function replies fastboot fail on error.
		return;
	}

	void *buf = fastboot_get_memory_buffer(fb, NULL);
	uint8_t data_len;

	int ret = ufs_read_descriptor(ctlr, (uint8_t)idn, (uint8_t)idx, buf,
				      FASTBOOT_MAX_DOWNLOAD_SIZE, &data_len);
	if (ret) {
		fastboot_fail(fb, "Failed to read UFS descriptor");
		return;
	}

	fb->memory_buffer_len = data_len;
	fb->has_staged_data = true;
	fastboot_succeed(fb);
}

static void fastboot_cmd_oem_write_ufs_descriptor(struct FastbootOps *fb,
						  const char *arg)
{
	if (!CONFIG(DRIVER_STORAGE_UFS)) {
		fastboot_fail(fb, "UFS is not supported by the board");
		return;
	}

	if (!fb->has_staged_data) {
		fastboot_fail(fb, "No staged data to write");
		return;
	}

	uint64_t data_len;
	void *buf = fastboot_get_memory_buffer(fb, &data_len);

	if (data_len > UFS_DESCRIPTOR_MAX_SIZE) {
		fastboot_fail(fb, "Staged data is too big");
		return;
	}

	UfsCtlr *ctlr = ufs_get_ctlr();
	if (!ctlr) {
		fastboot_fail(fb, "Could not find UFS controller");
		return;
	}

	uint32_t idn, idx;
	if (fastboot_parse_ufs_desc_args(fb, arg, &idn, &idx)) {
		// Parsing function replies fastboot fail on error.
		return;
	}

	int ret = ufs_write_descriptor(ctlr, (uint8_t)idn, (uint8_t)idx, buf,
				       data_len);
	if (ret) {
		fastboot_fail(fb, "Failed to write UFS descriptor");
		return;
	}

	fastboot_reset_staging(fb);

	fastboot_info(fb, "Reboot when you're done writing descriptors.");
	fastboot_succeed(fb);
}

#define BCB_RECOVERY_ARG0 "recovery"
#define BCB_RECOVERY_ARG_FASTBOOT "--fastboot"

static void fastboot_cmd_reboot_to_target(struct FastbootOps *fb, const char *arg)
{
	struct bootloader_message bcb;

	if (fastboot_disk_gpt_init(fb))
		return;

	if (android_misc_bcb_read(fb->disk, fb->gpt, &bcb)) {
		fastboot_info(fb, "Failed to read BCB from misc, trying to initialize new one");
		memset(&bcb, 0, sizeof(bcb));
	}

	/* Setup recovery arguments depending on the target */
	if (!strcmp("fastboot", arg)) {
		memset(bcb.command, 0, sizeof(bcb.command));
		memset(bcb.recovery, 0, sizeof(bcb.recovery));

		strcpy(bcb.command, BCB_CMD_BOOT_RECOVERY);
		snprintf(bcb.recovery, sizeof(bcb.recovery), "%s\n%s\n", BCB_RECOVERY_ARG0,
			 BCB_RECOVERY_ARG_FASTBOOT);
	} else if (!strcmp("recovery", arg)) {
		memset(bcb.command, 0, sizeof(bcb.command));
		memset(bcb.recovery, 0, sizeof(bcb.recovery));

		strcpy(bcb.command, BCB_CMD_BOOT_RECOVERY);
		snprintf(bcb.recovery, sizeof(bcb.recovery), "%s\n", BCB_RECOVERY_ARG0);
	} else if (!strcmp("bootloader", arg)) {
		memset(bcb.command, 0, sizeof(bcb.command));
		strcpy(bcb.command, BCB_CMD_BOOTONCE_BOOTLOADER);
	} else {
		fastboot_fail(fb, "Unknown reboot target");
		return;
	}

	if (android_misc_bcb_write(fb->disk, fb->gpt, &bcb)) {
		fastboot_fail(fb, "Failed to write reboot command to misc");
		return;
	}

	fastboot_succeed(fb);
	/*
	 * TODO(b/370988331): We should force boot from internal drive without rebooting
	 *                    to speed up this.
	 */
	fb->state = REBOOT;
}

static void fastboot_cmd_reboot(struct FastbootOps *fb, const char *arg)
{
	fastboot_succeed(fb);
	fb->state = REBOOT;
}

static void fastboot_cmd_set_active(struct FastbootOps *fb, const char *arg)
{
	if (fastboot_disk_gpt_init(fb))
		return;

	GptEntry *slot = fastboot_get_kernel_for_slot(fb->gpt, arg[0]);
	if (slot == NULL) {
		fastboot_fail(fb, "Could not find slot");
		return;
	}

	fastboot_slots_disable_all(fb->gpt);
	GptUpdateKernelWithEntry(fb->gpt, slot, GPT_UPDATE_ENTRY_ACTIVE);
	if (fastboot_save_gpt(fb))
		fastboot_fail(fb, "Could not save GPT to the disk");
	else
		fastboot_succeed(fb);
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
	CMD_ARGS("oem cmdline add", ' ', fastboot_cmd_oem_cmdline_add),
	CMD_ARGS("oem cmdline del", ' ', fastboot_cmd_oem_cmdline_del),
	CMD_ARGS("oem cmdline set", ' ', fastboot_cmd_oem_cmdline_set),
	CMD_NO_ARGS("oem cmdline", fastboot_cmd_oem_cmdline_get),
	CMD_ARGS("oem bootconfig add", ' ', fastboot_cmd_oem_bootconfig_add),
	CMD_ARGS("oem bootconfig del", ' ', fastboot_cmd_oem_bootconfig_del),
	CMD_ARGS("oem bootconfig set", ' ', fastboot_cmd_oem_bootconfig_set),
	CMD_NO_ARGS("oem bootconfig", fastboot_cmd_oem_bootconfig_get),
	CMD_NO_ARGS("oem get-kernels", fastboot_cmd_oem_get_kernels),
	CMD_ARGS("oem read-ufs-descriptor", ':', fastboot_cmd_oem_read_ufs_descriptor),
	CMD_ARGS("oem write-ufs-descriptor", ':', fastboot_cmd_oem_write_ufs_descriptor),
	CMD_ARGS("reboot", '-', fastboot_cmd_reboot_to_target),
	CMD_NO_ARGS("reboot", fastboot_cmd_reboot),
	CMD_ARGS("set_active", ':', fastboot_cmd_set_active),
	{
		.name = NULL,
	},
};
