/*
 * Copyright (C) 2015 Google Inc.
 *
 * Commands for testing x86 LPC I/O.
 */

#include "common.h"

static int do_ior(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int size;
	u16 port;

	if (argc < 2)
		return CMD_RET_USAGE;

	/* Check for size specification */
	size = cmd_get_data_size(argv[0], 1);
	if (size < 1)
		return CMD_RET_USAGE;

	/* Get IO port */
	port = strtoul(argv[1], NULL, 16);

	switch (size) {
	case 4:
		printf("0x%08x\n", inl(port));
		break;
	case 2:
		printf("0x%04x\n", inw(port));
		break;
	case 1:
		printf("0x%02x\n", inb(port));
		break;
	default:
		return CMD_RET_USAGE;
	}

	return CMD_RET_SUCCESS;
}

static int do_iow(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int size;
	u16 port;
	u32 value;

	if (argc < 3)
		return CMD_RET_USAGE;

	/* Check for size specification */
	size = cmd_get_data_size(argv[0], 1);
	if (size < 1)
		return CMD_RET_USAGE;

	/* Get IO port */
	port = strtoul(argv[1], NULL, 16);

	/* Get write value */
	value = strtoul(argv[2], NULL, 16);

	switch (size) {
	case 4:
		outl(value, port);
		break;
	case 2:
		outw((u16)value, port);
		break;
	case 1:
		outb((u8)value, port);
		break;
	default:
		return CMD_RET_USAGE;
	}

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	ior,	2,	1,
	"IO port input",
	"[.b, .w, .l] port"
);

U_BOOT_CMD(
	iow,	3,	1,
	"IO port output",
	"[.b, .w, .l] port value"
);
