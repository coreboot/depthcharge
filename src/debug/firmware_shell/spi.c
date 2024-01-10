/*
 * Command for accessing SPI inerface.
 *
 * Copyright 2014 The ChromiumOS Authors
 */

#include "common.h"
#include "drivers/flash/flash.h"

static int do_spi_read(int argc, char * const argv[], int read_flag)
{
	unsigned offset, length;
	char *buffer;
	void *mem_addr = 0;

	if (!read_flag && (argc != 2))
		return CMD_RET_USAGE;

	if (read_flag) {
		if (argc != 3)
			return CMD_RET_USAGE;
		mem_addr = (void *) strtoul(argv[2], 0, 16);
	}

	offset = strtoul(argv[0], 0, 16);
	length = strtoul(argv[1], 0, 16);

	buffer = xzalloc(length);
	if (flash_read(buffer, offset, length) != length) {
		free(buffer);
		console_printf("read failed!\n");
		return -1;
	}

	if (read_flag)
		memcpy(mem_addr, buffer, length);
	else
		print_buffer((unsigned long) mem_addr,
			     buffer, 1, length, 16);

	free(buffer);
	return 0;
}

static int do_spi_erase(int argc, char * const argv[], int ignore)
{
	unsigned offset, length;

	if (argc != 2)
		return CMD_RET_USAGE;

	offset = strtoul(argv[0], 0, 16);
	length = strtoul(argv[1], 0, 16);

	return length != flash_erase(offset, length);
}

static int do_spi_write(int argc, char * const argv[], int read_flag)
{
	unsigned offset, length;
	const void *mem_addr = 0;

	if (argc != 3)
		return CMD_RET_USAGE;

	mem_addr = (const void *) strtoul(argv[2], 0, 16);
	offset = strtoul(argv[0], 0, 16);
	length = strtoul(argv[1], 0, 16);

	return length != flash_write(mem_addr, offset, length);
}

static int do_spi(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	struct {
		const char *name;
		int param;
		int (*function)(int argc, char *const argv[], int flag);
	} subcommands[] = {
		{"read", 1, do_spi_read},
		{"dump", 0, do_spi_read},
		{"erase", 1, do_spi_erase},
		{"write", 0, do_spi_write}
	};
	int i;

	if (argc < 2)
		return CMD_RET_USAGE;

	for (i = 0; i < ARRAY_SIZE(subcommands); i++)
		if (!strcmp(subcommands[i].name, argv[1]))
			return subcommands[i].function(argc -2,
						       argv + 2,
						       subcommands[i].param);
	return CMD_RET_USAGE;
}

U_BOOT_CMD(
	spi,	5,	1,
	"SPI flash sub-system",
	"dump offset len      - dump on the console `len' bytes starting\n"
	"                           at `offset'\n"
	"spi erase offset len - Erase 'len' bytes starting at 'offset'\n"
	"spi read offset len addr - read 'len' bytes starting at 'offset'\n"
	"                           into RAM starting at 'addr'\n"
	"spi write offset len addr - write 'len' bytes starting at 'offset'\n"
	"                           from RAM starting at 'addr'\n"
);
