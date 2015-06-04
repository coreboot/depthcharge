/*
 * Copyright 2015 Google Inc.
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
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <assert.h>
#include <libpayload.h>
#include <stdint.h>
#include <vboot_nvstorage.h>

#include "base/gpt.h"
#include "base/init_funcs.h"
#include "boot/bcb.h"
#include "drivers/power/power.h"
#include "vboot/stages.h"

#define MAX_CMD_LENGTH	32
#define MAX_REC_LENGTH	768

struct bcb_data {
	BlockDev *bdev;
	size_t part_addr;
	size_t bcb_size_lba;
	size_t cmd_size_lba;
} bcb_data;

static uint16_t part_name[MAX_CMD_LENGTH];
static size_t part_name_len;
static uint8_t override_priority;

struct bcb {
	char cmd[MAX_CMD_LENGTH];
	char status[32];
	char rec[MAX_REC_LENGTH];
	char stage[32];
	char reserved[224];
} bcb_buffer;

/* If there is any failure in BCB handling, reboot into rec mode. */
static void bcb_fail(void)
{
	vboot_update_recovery(VBNV_RECOVERY_RW_BCB_ERROR);
	cold_reboot();
}

static BlockDev *bcb_find_bdev_from_ctrlr(BlockDevCtrlr *bdev_ctrlr)
{
	BlockDev *bdev;
	ListNode *devs;

	int count = get_all_bdevs(BLOCKDEV_FIXED, &devs);

	if (count == 0)
		bcb_fail();

	list_for_each(bdev, *devs, list_node) {
		if (bdev_ctrlr->ops.is_bdev_owned(&bdev_ctrlr->ops,
						  bdev)) {
			return bdev;
		}
	}

	/* Should never reach here. */
	bcb_fail();

	/* Added to make compiler happy. */
	return NULL;
}

static void bcb_init_data(void)
{
	/* Early return, if already initialized. */
	if (bcb_data.bdev)
		return;

	BlockDevCtrlr *bdev_ctrlr = bcb_board_bdev_ctrlr();

	if (bdev_ctrlr == NULL)
		bcb_fail();

	bcb_data.bdev = bcb_find_bdev_from_ctrlr(bdev_ctrlr);

	/* Allocate GPT data. */
	GptData *gpt = alloc_gpt(bcb_data.bdev);
	if (gpt == NULL)
		bcb_fail();

	/* Read GPT entries. */
	GptHeader *header = (GptHeader *)gpt->primary_header;
	GptEntry *entries = (GptEntry *)gpt->primary_entries;
	GptEntry *e;
	int i;

	/* BCB-partition label: "MSC" */
	const uint16_t lbl_required[] = {'M','S','C'};

	/* Identify the entry for BCB partition. */
	for (i = 0, e = entries; i < header->number_of_entries; i++, e++) {
		if (memcmp(e->name, lbl_required, sizeof(lbl_required)) == 0)
			break;
	}

	/* If BCB is not found, reboot into rec mode. */
	if (i == header->number_of_entries)
		bcb_fail();

	/* Ensure bcb partition is big enough to hold bcb data. */
	size_t part_size_lba;
	bcb_data.part_addr = e->starting_lba;
	part_size_lba = GptGetEntrySizeLba(e);

	bcb_data.cmd_size_lba = ALIGN_UP(MAX_CMD_LENGTH, gpt->sector_bytes);
	bcb_data.cmd_size_lba /= gpt->sector_bytes;

	bcb_data.bcb_size_lba = sizeof(struct bcb) / gpt->sector_bytes;

	if (bcb_data.bcb_size_lba > part_size_lba)
		bcb_fail();

	/* Cleanup GPT data */
	free_gpt(bcb_data.bdev, gpt);
}

/* Copy argument to cmd which is the name of the partition to be booted. */
static void bcb_copy_part_name(char *arg, size_t len)
{
	int i;

	/* arg is a NULL-terminated string containing partition label. */
	part_name_len = strnlen(arg, len);

	/*
	 * This arg will be compared against GPT labels to check which partition
	 * needs priority override. Since, GPT labels are stored in uint16_t[],
	 * copy arg to uint16_t[] to enable use of memcmp in
	 * bcb_override_priority().
	 */
	for (i = 0; i < part_name_len; i++)
		part_name[i] = arg[i];
	override_priority = 1;
}

static void bcb_reboot_bootloader(char *arg, size_t len)
{
	vboot_update_recovery(VBNV_RECOVERY_BCB_USER_MODE);
}

/* Post-cmd handle flags. */
enum {
	CLEAR_CMD = (1 << 0),
	REBOOT_DEVICE = (1 << 1),
	FLAGS_NONE = 0,
};

void bcb_handle_command(void)
{
	/* If device is in recovery, no need to initialize BCB. */
	if (vboot_in_recovery())
		return;

	/* Initialize data required to read/write BCB. */
	bcb_init_data();

	/* Read BCB partition. */
	BlockDevOps *ops = &bcb_data.bdev->ops;
	if (ops->read(ops, bcb_data.part_addr, bcb_data.bcb_size_lba,
		      (void *)&bcb_buffer) != bcb_data.bcb_size_lba)
		bcb_fail();

	/* List of expected cmds. */
	static const struct {
		/* cmd string */
		char name[MAX_CMD_LENGTH];
		/* callback to handle this cmd */
		void (*fnptr)(char *arg, size_t len);
		/* flags: indicate actions after handling cmd */
		uint32_t post_handle_flags;
	} cmd_table[] = {
		{"boot-", bcb_copy_part_name, FLAGS_NONE},
		{"bootonce-", bcb_copy_part_name, CLEAR_CMD},
		{"boot-bootloader", bcb_reboot_bootloader, REBOOT_DEVICE},
		{"bootonce-bootloader", bcb_reboot_bootloader,
		 CLEAR_CMD | REBOOT_DEVICE},
	};

	/* Pick the cmd with highest match len. */
	int match_len = -1, match_index = -1;
	int i;
	for (i = 0; i < ARRAY_SIZE(cmd_table); i++) {
		int len = strlen(cmd_table[i].name);

		if ((strncmp(bcb_buffer.cmd, cmd_table[i].name, len) == 0)
		    && (len > match_len)) {
			match_len = len;
			match_index = i;
		}
	}

	/* No cmd in BCB. */
	if (match_index == -1)
		return;

	/* Callback for the cmd, if present. */
	if (cmd_table[match_index].fnptr)
		cmd_table[match_index].fnptr(bcb_buffer.cmd + match_len,
					     MAX_CMD_LENGTH - match_len);

	/* Write back to BCB, if required. */
	if (cmd_table[match_index].post_handle_flags & CLEAR_CMD) {
		memset(bcb_buffer.cmd, 0, MAX_CMD_LENGTH);

		if (ops->write(ops, bcb_data.part_addr, bcb_data.cmd_size_lba,
			       (void *)&bcb_buffer.cmd)
		    != bcb_data.cmd_size_lba)
			bcb_fail();
	}

	/* Reboot device, if required. */
	if (cmd_table[match_index].post_handle_flags & REBOOT_DEVICE)
		cold_reboot();
}

uint8_t bcb_override_priority(const uint16_t *name)
{
	/*
	 * If BCB partition requests boot into a particular partition,
	 * override its priority.
	 */
	if (override_priority &&
	    (memcmp(part_name, name, part_name_len) == 0))
		return 1;

	return 0;
}

void bcb_request_recovery(const char *cmd, const char *rec)
{
	/* Ensure correct lengths for cmd and rec. */
	assert(strlen(cmd) < MAX_CMD_LENGTH);
	assert(strlen(rec) < MAX_REC_LENGTH);

	/* Initialize data required to write BCB. */
	bcb_init_data();

	/* Fill in bcb buffer. */
	memset(&bcb_buffer, 0, sizeof(bcb_buffer));
	strcpy(bcb_buffer.cmd, cmd);
	strcpy(bcb_buffer.rec, rec);

	/* Write bcb buffer to bdev. */
	BlockDevOps *ops = &bcb_data.bdev->ops;
	if (ops->write(ops, bcb_data.part_addr, bcb_data.bcb_size_lba,
		       (void *)&bcb_buffer) != bcb_data.bcb_size_lba)
		bcb_fail();
}
