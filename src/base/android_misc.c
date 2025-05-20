// SPDX-License-Identifier: GPL-2.0
/* Copyright 2025 Google LLC. */

#include <stdlib.h>

#include "base/android_misc.h"
#include "base/gpt.h"
#include "gpt_misc.h"

static int android_misc_read(BlockDev *disk, GptData *gpt, size_t blocks_offset, void *buf,
			     size_t size)
{
	if (gpt_read_partition(disk, gpt, GptPartitionNames[GPT_ANDROID_MISC],
			       blocks_offset, buf, size) != GPT_IO_SUCCESS) {
		printf("Failed to read misc partition (offset %zu, size %zu)\n",
		       blocks_offset * disk->block_size, size);
		return -1;
	}

	return 0;
}

static int android_misc_write(BlockDev *disk, GptData *gpt, size_t blocks_offset, void *buf,
			      size_t size)
{
	if (gpt_write_partition(disk, gpt, GptPartitionNames[GPT_ANDROID_MISC],
			    blocks_offset, buf, size) != GPT_IO_SUCCESS) {
		printf("Failed to write misc partition (offset %zu, size %zu)\n",
		       blocks_offset * disk->block_size, size);
		return -1;
	}

	return 0;
}

int android_misc_bcb_write(BlockDev *disk, GptData *gpt, struct bootloader_message *bcb)
{
	android_misc_write(disk, gpt, 0, bcb, sizeof(*bcb));

	return 0;
}

enum android_misc_bcb_command android_misc_get_bcb_command(BlockDev *disk, GptData *gpt)
{
	struct bootloader_message bcb;

	if (android_misc_read(disk, gpt, 0, &bcb, sizeof(bcb)))
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
