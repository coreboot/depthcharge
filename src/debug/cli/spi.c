/*
 * Command for accessing SPI inerface.
 *
 * Copyright (C) 2014 Chromium OS Authors
 */

#include "common.h"
#include "drivers/flash/flash.h"

static int do_spi(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	unsigned offset, length, i = 2;
	const char *buffer;
	void *mem_addr = 0;
	int read_flag = 0;	/* True if the subcommand is 'read' */

	if (argc < 2)
		return CMD_RET_USAGE;

	if (!strcmp(argv[1], "dump")) {
		if (argc != 4)
			return CMD_RET_USAGE;
	} else if (!strcmp(argv[1], "read")) {
		if (argc != 5)
			return CMD_RET_USAGE;
		mem_addr = (void *) strtoul(argv[2], 0, 16);
		i = 3;
		read_flag = 1;
	} else {
		return CMD_RET_USAGE;
	}

	offset = strtoul(argv[i++], 0, 16);
	length = strtoul(argv[i++], 0, 16);

	buffer = flash_read(offset, length);

	if (!buffer) {
		printf("read failed!\n");
		return -1;
	}

	if (read_flag)
		memcpy(mem_addr, buffer, length);
	else
		print_buffer((unsigned long) mem_addr,
			     buffer, 1, length, 16);

	return 0;
}

U_BOOT_CMD(
	spi,	5,	1,
	"SPI flash sub-system",
	"dump offset len      - dump on the console `len' bytes starting\n"
	"                           at `offset'\n"
	"spi read offset len addr - read 'len' bytes starting at 'offset'\n"
	"                           into RAM starting at 'addr'\n"
);
