/*
 * Commands for testing image drawing
 *
 * Copyright (C) 2015 Chromium OS Authors
 */

#include <cbfs.h>
#include "common.h"
#include "drivers/video/coreboot_fb.h"

static int do_draw_image(char * const name, unsigned long x, unsigned long y)
{
	struct cbfs_file *file;

	file = cbfs_get_file(CBFS_DEFAULT_MEDIA, name);
	if (file == NULL) {
		printf("File '%s' not found\n", name);
		return CMD_RET_FAILURE;
	}

	if (dc_corebootfb_draw_bitmap(x, y, (void *)file + ntohl(file->offset)))
		return CMD_RET_FAILURE;

	return CMD_RET_SUCCESS;
}

static int do_draw(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int rv = CMD_RET_FAILURE;

	if (argc == 4) {
		unsigned long x = strtoul(argv[2], NULL, 10);
		unsigned long y = strtoul(argv[3], NULL, 10);
		/* TODO: validate (x, y) fits in the screen */
		rv = do_draw_image(argv[1], x, y);
	} else {
		printf("Syntax error\n");
		rv = CMD_RET_USAGE;
	}

	return rv;
}

U_BOOT_CMD(
	   draw,	4,	1,
	   "draw image on screen",
	   "draw <name> <x> <y> - load image from cbfs and draw it at (x, y)"
);
