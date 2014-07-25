/*
 * Copyright (c) 2014, Linux Foundation. All rights reserved
 * Copyright 2014 Chromium OS Authors
 */

#include "base/list.h"
#include "debug/cli/common.h"
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
		printf("%c %2d: %s\n",
		       current_devices.curr_device == i ? '*' : ' ',
		       i, (*bd)->name ? (*bd)->name : "UNNAMED");

	printf("%d devices total\n", i);
	return 0;
}

static int storage_read(int argc, char *const argv[])
{
	int base_block, num_blocks, *dest_addr, i;
	int *args[] = {&base_block, &num_blocks, (int*) &dest_addr};
	BlockDev *bd;

	for (i = 0; i < ARRAY_SIZE(args); i++)
		*args[i] = strtoul(argv[i], NULL, 0);

	if ((current_devices.curr_device < 0) ||
	    (current_devices.curr_device >= current_devices.total)) {
		printf("Is storage subsystem initialized?");
		return -1;
	}

	bd = current_devices.known_devices[current_devices.curr_device];
	i = bd->ops.read(&bd->ops, base_block, num_blocks, dest_addr);
	return i != num_blocks;
}

static int storage_write(int argc, char *const argv[])
{
	int base_block, num_blocks, *src_addr, i;
	int *args[] = {&base_block, &num_blocks, (int *) &src_addr};
	BlockDev *bd;

	for (i = 0; i < ARRAY_SIZE(args); i++)
		*args[i] = strtoul(argv[i], NULL, 0);

	if ((current_devices.curr_device < 0) ||
	    (current_devices.curr_device >= current_devices.total)) {
		printf("Is storage subsystem initialized?");
		return -1;
	}

	bd = current_devices.known_devices[current_devices.curr_device];
	i = bd->ops.write(&bd->ops, base_block, num_blocks, src_addr);
	return i != num_blocks;
}

static int storage_dev(int argc, char *const argv[])
{
	int rv = 0;

	if (!current_devices.total) {
		printf("No initialized devices present\n");
	} else {
		unsigned long cur_device;

		cur_device = argc ?
			strtoul(argv[0], NULL, 0) :
			current_devices.curr_device;
		if (cur_device >= current_devices.total) {
			printf("%d: bad device index. Current devices:",
			       (int)cur_device);
			storage_show(0, NULL);
			rv = -1;
		} else {
			current_devices.curr_device = cur_device;
			printf("%s\n",
			       current_devices.known_devices[cur_device]->name);
		}
	}
	return rv;
}

static int storage_init(int argc, char *const argv[])
{
	int i, count;
	const ListNode *controllers[] = {
		&fixed_block_dev_controllers,
		&removable_block_dev_controllers
	};

	const ListNode *devices[] = {
		&fixed_block_devices,
		&removable_block_devices
	};

	for (i = 0; i < ARRAY_SIZE(controllers); i++)
		for (const ListNode *node = controllers[i]->next;
		     node;
		     node = node->next) {
			BlockDevCtrlr *bdc;

			bdc = container_of(node, BlockDevCtrlr, list_node);
			if (bdc->ops.update && bdc->need_update)
				bdc->ops.update(&bdc->ops);
			count++;
		}

	for (count = i = 0; i < ARRAY_SIZE(devices); i++)
		for (const ListNode *node = devices[i]->next;
		     node;
		     node = node->next) {
			BlockDev *bd;

			bd = container_of(node, BlockDev, list_node);
			current_devices.known_devices[count++] = bd;
		}

	current_devices.total = count;
	current_devices.curr_device = 0;

	return storage_show(0, NULL);
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
	storage, CONFIG_SYS_MAXARGS,	1,
	"command for controlling onboard storage devices",
	"\n"
	" dev [dev#] - display or set default storage device\n"
	" init - initialize storage devices\n"
	" show - show currently initialized devices\n"
	" read <base blk> <num blks> <dest addr> - read from default device\n"
	" write <base blk> <num blks> <src addr> - write from default device\n"
);

