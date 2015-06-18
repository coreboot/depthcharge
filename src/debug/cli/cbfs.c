/*
 * Commands for testing coreboot file system
 *
 * Copyright (C) 2015 Chromium OS Authors
 */

#include "common.h"
#include <cbfs.h>

static int do_cbfs_dump(char * const name)
{
	struct cbfs_file *file;

	file = cbfs_get_file(CBFS_DEFAULT_MEDIA, name);
	if (file == NULL) {
		printf("File '%s' not found\n", name);
		return CMD_RET_FAILURE;
	}

	printf("Dumping '%s' at %p size=%u type=%u\n",
	       name, (void *)file + ntohl(file->offset),
	       ntohl(file->len), ntohl(file->type));
	hexdump((void *)file + ntohl(file->offset), ntohl(file->len));

	return CMD_RET_SUCCESS;
}

static int do_cbfs_ls()
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
	   "cbfs dump <name> - dump file content"
	   "cbfs ls - list files in cbfs"
);
