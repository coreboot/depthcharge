/*
 * Copyright (c) 2014, Linux Foundation. All rights reserved
 * Copyright 2014 The ChromiumOS Authors
 */

#include <assert.h>
#include <commonlib/list.h>
#include <ctype.h>
#include <vb2_api.h>
#include <vb2_gpt.h>
#include <vboot_api.h>

#include "debug/firmware_shell/common.h"
#include "drivers/storage/blockdev.h"

typedef struct {

	/* max 10 storage devices is enough */
	BlockDev *known_devices[10];
	int curr_device;
	int total;

} storage_devices;

static storage_devices current_devices;

static int storage_show(int argc, char *const argv[])
{
	int i;
	BlockDev **bd;

	for (i = 0, bd = current_devices.known_devices;
	     i < current_devices.total;
	     i++, bd++)
		console_printf("%c %2d: %s\n",
		       current_devices.curr_device == i ? '*' : ' ',
		       i, (*bd)->name ? (*bd)->name : "UNNAMED");

	console_printf("%d devices total\n", i);
	return 0;
}

static int storage_read(int argc, char *const argv[])
{
	lba_t base_block, num_blocks, rc;
	uintptr_t dest_addr;
	BlockDev *bd;

	assert(argc >= 3);

	base_block = strtoull(argv[0], NULL, 0);
	num_blocks = strtoull(argv[1], NULL, 0);
	dest_addr = strtoull(argv[2], NULL, 0);

	if ((current_devices.curr_device < 0) ||
	    (current_devices.curr_device >= current_devices.total)) {
		console_printf("Is storage subsystem initialized?");
		return -1;
	}

	bd = current_devices.known_devices[current_devices.curr_device];
	rc = bd->ops.read(&bd->ops, base_block, num_blocks, (void *)dest_addr);
	return rc != num_blocks;
}

static int storage_write(int argc, char *const argv[])
{
	lba_t base_block, num_blocks, rc;
	uintptr_t src_addr;
	BlockDev *bd;

	assert(argc >= 3);

	base_block = strtoull(argv[0], NULL, 0);
	num_blocks = strtoull(argv[1], NULL, 0);
	src_addr = strtoull(argv[2], NULL, 0);

	if ((current_devices.curr_device < 0) ||
	    (current_devices.curr_device >= current_devices.total)) {
		console_printf("Is storage subsystem initialized?");
		return -1;
	}

	bd = current_devices.known_devices[current_devices.curr_device];
	rc = bd->ops.write(&bd->ops, base_block, num_blocks, (void *)src_addr);
	return rc != num_blocks;
}

static int storage_erase(int argc, char *const argv[])
{
	lba_t base_block, num_blocks, rc;
	BlockDev *bd;

	assert(argc >= 2);

	base_block = strtoull(argv[0], NULL, 0);
	num_blocks = strtoull(argv[1], NULL, 0);

	if ((current_devices.curr_device < 0) ||
	    (current_devices.curr_device >= current_devices.total)) {
		console_printf("Is storage subsystem initialized?");
		return -1;
	}

	bd = current_devices.known_devices[current_devices.curr_device];
	if (!bd->ops.erase) {
		console_printf("Erase not applicable to %s\n", bd->name);
		return CMD_RET_SUCCESS;
	}

	rc = bd->ops.erase(&bd->ops, base_block, num_blocks);
	return rc != num_blocks;
}

static int storage_dev(int argc, char *const argv[])
{
	int rv = 0;

	if (!current_devices.total) {
		console_printf("No initialized devices present\n");
	} else {
		unsigned long cur_device;

		cur_device = argc ?
			strtoul(argv[0], NULL, 0) :
			current_devices.curr_device;
		if (cur_device >= current_devices.total) {
			console_printf("%d: bad device index. Current devices:",
			       (int)cur_device);
			storage_show(0, NULL);
			rv = -1;
		} else {
			current_devices.curr_device = cur_device;
			console_printf("%s\n",
			       current_devices.known_devices[cur_device]->name);
		}
	}
	return rv;
}

static int storage_init(int argc, char *const argv[])
{
	const blockdev_type_t bdev_types[] = {
		BLOCKDEV_FIXED,
		BLOCKDEV_REMOVABLE,
	};
	int count = 0;

	for (int i = 0; i < ARRAY_SIZE(bdev_types); i++) {
		struct list_node *bdevs;
		BlockDev *bd;

		get_all_bdevs(bdev_types[i], &bdevs);
		list_for_each(bd, *bdevs, list_node) {
			if (count >= ARRAY_SIZE(current_devices.known_devices)) {
				console_printf("Error: Number of block devices exceeds %zu\n",
					       ARRAY_SIZE(current_devices.known_devices));
				return -1;
			}
			current_devices.known_devices[count++] = bd;
		}
	}

	current_devices.total = count;
	current_devices.curr_device = 0;

	return storage_show(0, NULL);
}

static BlockDev *storage_get_current_bdev(void)
{
	BlockDev *bdev, *current_bdev;
	struct list_node *devs;

	if (!current_devices.total) {
		if (storage_init(0, NULL) || !current_devices.total) {
			console_printf("No initialized devices present\n");
			return NULL;
		}
	}

	current_bdev =
		current_devices.known_devices[current_devices.curr_device];

	get_all_bdevs(current_bdev->removable ? BLOCKDEV_REMOVABLE
					      : BLOCKDEV_FIXED,
		      &devs);

	/* Ensure current_bdev is still alive */
	list_for_each(bdev, *devs, list_node)
		if (bdev == current_bdev)
			break;

	if (bdev != current_bdev) {
		console_printf("Failed to get block device for current device\n");
		return NULL;
	}

	return bdev;
}

static void print_entry_name(const GptEntry *entry)
{
	const uint16_t *name = entry->name;

	/* Crude wide-char print */
	for (size_t j = 0; name[j] && j < ARRAY_SIZE(entry->name); j++)
		console_printf("%c", name[j] & 0xff);
}

static int storage_part(int argc, char *const argv[])
{
	BlockDev *bdev;
	GptData gpt = {};
	GptHeader *header;
	GptEntry *entry;

	const Guid guid_unused = GPT_ENT_TYPE_UNUSED;

	bdev = storage_get_current_bdev();
	if (!bdev)
		return CMD_RET_FAILURE;

	gpt.sector_bytes = bdev->block_size;
	gpt.drive_sectors = bdev->block_count;
	gpt.flags = 0;

	if (AllocAndReadGptData(bdev, &gpt)) {
		console_printf("Unable to read GPT data\n");
		return CMD_RET_FAILURE;
	}

	if (GptInit(&gpt) != GPT_SUCCESS) {
		console_printf("Unable to parse GPT\n");
		WriteAndFreeGptData(bdev, &gpt);
		return CMD_RET_FAILURE;
	}

	header = (GptHeader *)gpt.primary_header;
	entry = (GptEntry *)gpt.primary_entries;

	console_printf("------------ GPT for %s ------------\n\n", bdev->name);
	console_printf("Bytes per LBA = %u\n\n", bdev->block_size);

	console_printf("SNo: %18s %18s Name\n", "Start", "Count");
	for (int i = 0; i < header->number_of_entries; i++, entry++) {
		if (memcmp(&entry->type, &guid_unused, sizeof(Guid)) == 0)
			continue;

		console_printf("%3d: %#18llx %#18llx ", i + 1,
			entry->starting_lba,
			entry->ending_lba - entry->starting_lba + 1);

		print_entry_name(entry);
		if (IsBootableEntry(entry))
			console_printf(" [S=%d P=%d T=%d]",
				       GetEntrySuccessful(entry),
				       GetEntryPriority(entry),
				       GetEntryTries(entry));
		console_printf("\n");
	}

	WriteAndFreeGptData(bdev, &gpt);
	return CMD_RET_SUCCESS;
}

static bool entry_matches_target(GptEntry *entry, const char *target)
{
	if (GptEntryHasName(entry, target, NULL))
		return true;

	size_t len = strlen(target);
	if (len == 1 || (len == 2 && (target[0] == '_' || target[0] == '-'))) {
		char slot = tolower((unsigned char)target[len - 1]);
		char android_suffix[] = { '_', slot, '\0' };
		char cros_name[] = { 'K', 'E', 'R', 'N', '-',
				     toupper((unsigned char)slot), '\0' };

		if (IsAndroid(entry) &&
		    GptEntryHasName(entry, GptPartitionNames[GPT_ANDROID_VBMETA],
				    android_suffix))
			return true;

		if (IsChromeOS(entry) && GptEntryHasName(entry, cros_name, NULL))
			return true;
	}

	return false;
}

static int storage_restore_boot(int argc, char *const argv[])
{
	BlockDev *bdev;
	GptData gpt = {};
	GptHeader *header;
	GptEntry *entry;
	const char *target = argc > 0 ? argv[0] : NULL;
	int restored = 0;

	bdev = storage_get_current_bdev();
	if (!bdev)
		return CMD_RET_FAILURE;

	gpt.sector_bytes = bdev->block_size;
	gpt.drive_sectors = bdev->block_count;
	gpt.flags = 0;

	if (AllocAndReadGptData(bdev, &gpt)) {
		console_printf("Unable to read GPT data\n");
		return CMD_RET_FAILURE;
	}

	if (GptInit(&gpt) != GPT_SUCCESS) {
		console_printf("Unable to parse GPT\n");
		WriteAndFreeGptData(bdev, &gpt);
		return CMD_RET_FAILURE;
	}

	header = (GptHeader *)gpt.primary_header;
	entry = (GptEntry *)gpt.primary_entries;

	for (int i = 0; i < header->number_of_entries; i++, entry++) {
		if (!IsBootableEntry(entry))
			continue;

		if (target) {
			if (!entry_matches_target(entry, target)) {
				if (GetEntryPriority(entry) >= 2) {
					SetEntryPriority(entry, 1);
					GptModified(&gpt);
				}
				continue;
			}
			GptUpdateKernelWithEntry(&gpt, entry,
						 GPT_UPDATE_ENTRY_ACTIVE);
		} else {
			int prio = (restored == 0) ? 2 : 1;

			SetEntrySuccessful(entry, 1);
			SetEntryPriority(entry, prio);
			SetEntryTries(entry, 0);
			GptModified(&gpt);
		}

		restored++;
		console_printf("Restored ");
		print_entry_name(entry);
		console_printf(" [S=%d P=%d T=%d]\n",
			       GetEntrySuccessful(entry),
			       GetEntryPriority(entry),
			       GetEntryTries(entry));
	}

	if (!restored) {
		console_printf("No matching bootable partition found\n");
		WriteAndFreeGptData(bdev, &gpt);
		return CMD_RET_FAILURE;
	}

	if (WriteAndFreeGptData(bdev, &gpt)) {
		console_printf("Failed to write GPT data to disk\n");
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

typedef struct {
	const char *subcommand_name;
	int (*subcmd)(int argc, char *const argv[]);
	int min_arg;
	int max_arg;
} cmd_map;

static const cmd_map cmdmap[] = {
	{ "dev", storage_dev, 0, 1 },
	{ "init", storage_init, 0, 0 },
	{ "show", storage_show, 0, 0 },
	{ "read", storage_read, 3, 3 },
	{ "write", storage_write, 3, 3 },
	{ "erase", storage_erase, 2, 2 },
	{ "part", storage_part, 0, 0 },
	{ "restore", storage_restore_boot, 0, 1 },
};

static int do_storage(cmd_tbl_t *cmdtp, int flag,
		      int argc, char * const argv[])
{
	if (argc >= 2) {
		int i;

		for (i = 0; i < ARRAY_SIZE(cmdmap); i++)
			if (!strcmp(argv[1], cmdmap[i].subcommand_name)) {
				int nargs = argc - 2;

				if ((cmdmap[i].min_arg <= nargs) &&
				    (cmdmap[i].max_arg >= nargs))
					return cmdmap[i].subcmd(nargs, argv + 2);
			}
	}
	return CMD_RET_USAGE;
}

U_BOOT_CMD(
	storage, SYS_MAXARGS,	1,
	"command for controlling onboard storage devices",
	"\n"
	" dev [dev#] - display or set default storage device\n"
	" erase <base blk> <num blks> - erase in default device\n"
	" init - initialize storage devices\n"
	" part - display GPT partition table and boot attributes\n"
	" restore [slot|part] - restore OS boot metadata (clear failed flag)\n"
	" show - show currently initialized devices\n"
	" read <base blk> <num blks> <dest addr> - read from default device\n"
	" write <base blk> <num blks> <src addr> - write to default device\n"
);
