// SPDX-License-Identifier: GPL-2.0
/* Copyright 2025 Google LLC. */

#include <commonlib/bsd/ipchksum.h>
#include <stdlib.h>

#include "base/android_misc.h"
#include "base/gpt.h"
#include "boot/bootconfig.h"
#include "gpt_misc.h"

/* Part of the structure used to calculate the checksum */
#define ANDROID_MISC_OEM_CMDLINE_CHECKSUM_SIZE(cmd) \
	(sizeof(cmd) - sizeof((cmd).data) + (cmd).cmdline_len + (cmd).bootconfig_len)

bool is_android_misc_cmd_separator(const char c, bool bootconfig)
{
	if (bootconfig)
		return c == '\n' || c == ';';

	return isspace(c);
}

bool is_android_misc_oem_cmdline_valid(struct android_misc_oem_cmdline *cmd)
{
	char *cmdline_end;
	char *bootcfg_end;

	if (cmd->magic != ANDROID_MISC_OEM_CMDLINE_MAGIC) {
		printf("Wrong android_misc_oem_cmdline magic (got: 0x%x, expected 0x%x)\n",
		       cmd->magic, ANDROID_MISC_OEM_CMDLINE_MAGIC);
		return false;
	}
	if (cmd->version != 0) {
		printf("Unknown android_misc_oem_cmdline version (%d)\n", cmd->version);
		return false;
	}
	if (cmd->cmdline_len + cmd->bootconfig_len > sizeof(cmd->data) ||
	    cmd->cmdline_len < 1 || cmd->bootconfig_len < 1) {
		printf("Wrong android_misc_oem_cmdline len (c %d, b %d, total %d)\n",
		       cmd->cmdline_len, cmd->bootconfig_len,
		       cmd->cmdline_len + cmd->bootconfig_len);
		return false;
	}
	cmdline_end = android_misc_get_oem_cmd(cmd, false) + cmd->cmdline_len - 1;
	bootcfg_end = android_misc_get_oem_cmd(cmd, true) + cmd->bootconfig_len - 1;
	if (*cmdline_end != '\0' || *bootcfg_end != '\0' ||
	    (cmd->cmdline_len > 1 && !is_android_misc_cmd_separator(cmdline_end[-1], false)) ||
	    (cmd->bootconfig_len > 1 &&
	     !is_android_misc_cmd_separator(bootcfg_end[-1], true))) {
		printf("Wrong ending of android_misc_oem_cmdline\n");
		return false;
	}
	if (ipchksum(cmd, ANDROID_MISC_OEM_CMDLINE_CHECKSUM_SIZE(*cmd))) {
		printf("Wrong android_misc_oem_cmdline checksum\n");
		return false;
	}

	return true;
}

int android_misc_oem_cmdline_update_checksum(struct android_misc_oem_cmdline *cmd)
{
	if (cmd->cmdline_len + cmd->bootconfig_len > sizeof(cmd->data)) {
		printf("Wrong android_misc_oem_cmdline len (%d)\n",
		       cmd->cmdline_len + cmd->bootconfig_len);
		return -1;
	}

	cmd->chksum = 0;
	cmd->chksum = ipchksum(cmd, ANDROID_MISC_OEM_CMDLINE_CHECKSUM_SIZE(*cmd));

	return 0;
}

static int android_misc_read(BlockDev *disk, GptData *gpt, size_t offset, void *buf,
			     size_t size)
{
	if (gpt_read_partition(disk, gpt, GptPartitionNames[GPT_ANDROID_MISC],
			       offset, buf, size) != GPT_IO_SUCCESS) {
		printf("Failed to read misc partition (offset %zu, size %zu)\n",
		       offset * disk->block_size, size);
		return -1;
	}

	return 0;
}

static int android_misc_write(BlockDev *disk, GptData *gpt, size_t offset, void *buf,
			      size_t size)
{
	if (gpt_write_partition(disk, gpt, GptPartitionNames[GPT_ANDROID_MISC], offset,
				buf, size) != GPT_IO_SUCCESS) {
		printf("Failed to write misc partition (offset %zu, size %zu)\n",
		       offset * disk->block_size, size);
		return -1;
	}

	return 0;
}

int android_misc_oem_cmdline_write(BlockDev *disk, GptData *gpt,
				   struct android_misc_oem_cmdline *cmd)
{
	if (android_misc_oem_cmdline_update_checksum(cmd))
		return -1;

	return android_misc_write(disk, gpt, MISC_VENDOR_SPACE_OEM_CMDLINE_OFFSET,
				  cmd, sizeof(*cmd));
}

int android_misc_oem_cmdline_read(BlockDev *disk, GptData *gpt,
				  struct android_misc_oem_cmdline *cmd)
{
	if (android_misc_read(disk, gpt, MISC_VENDOR_SPACE_OEM_CMDLINE_OFFSET,
			      cmd, sizeof(*cmd)))
		return -1;

	if (!is_android_misc_oem_cmdline_valid(cmd))
		return -1;

	return 0;
}

char *android_misc_get_oem_cmd(struct android_misc_oem_cmdline *cmd, bool bootconfig)
{
	if (bootconfig)
		return &cmd->data[cmd->cmdline_len];
	else
		return cmd->data;
}

int android_misc_append_oem_cmd(struct android_misc_oem_cmdline *cmd, const char *buf,
			     bool bootconfig)
{
	const int buf_len = strlen(buf) + 1;
	char *append_to;

	/* Empty string, nothing to do */
	if (buf_len == 1)
		return 0;

	if (buf_len > sizeof(cmd->data) - cmd->cmdline_len - cmd->bootconfig_len) {
		printf("Not enough space in android_oem_cmdline\n");
		return -1;
	}

	if (bootconfig) {
		append_to = &cmd->data[cmd->cmdline_len + cmd->bootconfig_len - 1];
		append_to[buf_len - 1] = BOOTCONFIG_DELIMITER;
		cmd->bootconfig_len += buf_len;
	} else {
		append_to = &cmd->data[cmd->cmdline_len - 1];
		memmove(append_to + buf_len + 1, android_misc_get_oem_cmd(cmd, true),
			cmd->bootconfig_len);
		append_to[buf_len - 1] = ' ';
		cmd->cmdline_len += buf_len;
	}
	memcpy(append_to, buf, buf_len - 1);
	append_to[buf_len] = '\0';

	return 0;
}

int android_misc_del_oem_cmd(struct android_misc_oem_cmdline *cmd, const size_t idx,
			     const size_t len, bool bootconfig)
{
	char *to_del;
	size_t following_data;

	to_del = android_misc_get_oem_cmd(cmd, bootconfig) + idx;
	if (bootconfig) {
		if (idx >= cmd->bootconfig_len || cmd->bootconfig_len - idx <= len) {
			printf("Index and length outside of bootconfig\n");
			return -1;
		}
		cmd->bootconfig_len -= len;
		following_data = cmd->bootconfig_len - idx;
	} else {
		if (idx >= cmd->cmdline_len || cmd->cmdline_len - idx <= len) {
			printf("Index and length outside of cmdline\n");
			return -1;
		}
		cmd->cmdline_len -= len;
		following_data = cmd->cmdline_len - idx + cmd->bootconfig_len;
	}
	memmove(to_del, to_del + len, following_data);

	return 0;
}

int android_misc_set_oem_cmd(struct android_misc_oem_cmdline *cmd, const char *buf,
			     bool bootconfig)
{
	const int buf_len = strlen(buf);

	if (bootconfig) {
		if (buf_len > sizeof(cmd->data) - cmd->cmdline_len - 1) {
			printf("Not enough space in android oem bootconfig\n");
			return -1;
		}
		if (android_misc_del_oem_cmd(cmd, 0, cmd->bootconfig_len - 1, true))
			return -1;
	} else {
		if (buf_len > sizeof(cmd->data) - cmd->bootconfig_len - 1) {
			printf("Not enough space in android oem cmdline\n");
			return -1;
		}
		if (android_misc_del_oem_cmd(cmd, 0, cmd->cmdline_len - 1, false))
			return -1;
	}

	return android_misc_append_oem_cmd(cmd, buf, bootconfig);
}

void android_misc_reset_oem_cmd(struct android_misc_oem_cmdline *cmd)
{
	cmd->magic = ANDROID_MISC_OEM_CMDLINE_MAGIC;
	cmd->version = 0;
	cmd->data[0] = '\0';
	cmd->cmdline_len = 1;
	cmd->data[1] = '\0';
	cmd->bootconfig_len = 1;
	android_misc_oem_cmdline_update_checksum(cmd);
}

int android_misc_bcb_write(BlockDev *disk, GptData *gpt, struct bootloader_message *bcb)
{
	return android_misc_write(disk, gpt, 0, bcb, sizeof(*bcb));
}

int android_misc_bcb_read(BlockDev *disk, GptData *gpt, struct bootloader_message *bcb)
{
	return android_misc_read(disk, gpt, 0, bcb, sizeof(*bcb));
}

enum android_misc_bcb_command android_misc_get_bcb_command(BlockDev *disk, GptData *gpt)
{
	struct bootloader_message bcb;

	if (android_misc_bcb_read(disk, gpt, &bcb))
		return MISC_BCB_DISK_ERROR;

	/* BCB command field is for the bootloader */
	if (!strcmp(bcb.command, BCB_CMD_BOOT_RECOVERY)) {
		/* Android recovery will clear the command after boot */
		return MISC_BCB_RECOVERY_BOOT;
	} else if (!strcmp(bcb.command, BCB_CMD_BOOTONCE_BOOTLOADER)) {
		/* Command field is supposed to be a one-shot thing. Clear it. */
		memset(bcb.command, 0, sizeof(bcb.command));
		android_misc_bcb_write(disk, gpt, &bcb);
		return MISC_BCB_BOOTLOADER_BOOT;
	} else if (bcb.command[0] == '\0') {
		/* If empty just boot normally */
		return MISC_BCB_NORMAL_BOOT;
	}

	printf("Unknown boot command \"%.*s\"\n",
	       (int)sizeof(bcb.command), bcb.command);
	return MISC_BCB_UNKNOWN;
}

int android_misc_system_space_write(BlockDev *disk, GptData *gpt,
				    struct misc_system_space *space)
{
	return android_misc_write(disk, gpt, MISC_SYSTEM_SPACE_OFFSET, space, sizeof(*space));
}

int android_misc_system_space_read(BlockDev *disk, GptData *gpt,
				   struct misc_system_space *space)
{
	return android_misc_read(disk, gpt, MISC_SYSTEM_SPACE_OFFSET, space, sizeof(*space));
}
