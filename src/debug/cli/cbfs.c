/*
 * Commands for testing coreboot file system
 *
 * Copyright 2015 The ChromiumOS Authors
 */

#include "common.h"
#include <cbfs.h>

static int do_cbfs_dump(char * const name)
{
	void *file;
	size_t size;

	file = cbfs_map(name, &size);
	if (file == NULL) {
		printf("File '%s' not found\n", name);
		return CMD_RET_FAILURE;
	}

	printf("Dumping '%s' size=%lu type=%u\n", name, size,
	       cbfs_get_type(name));
	hexdump((void *)file, size);
	free(file);

	return CMD_RET_SUCCESS;
}

static int do_cbfs_ls(void)
{
	printf("Command not implemented\n");
	return CMD_RET_SUCCESS;
}

static int do_cbfs(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	if (!strcmp(argv[1], "dump") && argc == 3)
		return do_cbfs_dump(argv[2]);
	else if (!strcmp(argv[1], "ls"))
		return do_cbfs_ls();

	printf("Syntax error\n");
	return CMD_RET_USAGE;
}

U_BOOT_CMD(
	   cbfs,	3,	1,
	   "examine coreboot file system",
	   "dump <name> - dump file content\n"
	   "ls - list files in cbfs"
);
