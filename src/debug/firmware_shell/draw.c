/*
 * Commands for testing graphics drawing
 *
 * Copyright 2015 The ChromiumOS Authors
 */

#include <libpayload.h>
#include <cbfs.h>
#include <coreboot_tables.h>
#include "common.h"
#include "drivers/video/display.h"
#include "drivers/video/coreboot_fb.h"

static int initialized = 0;

static int init_display(void)
{
	if (initialized)
		return 0;
	if (display_init())
		return -1;
	if (backlight_update(1))
		return -1;
	/* initialize video console */
	video_init();
	video_console_clear();
	video_console_cursor_enable(0);

	initialized = 1;
	return 0;
}

static int do_draw_image(int argc, char * const argv[])
{
	const struct scale *dim_rel = NULL;
	void *bitmap;
	size_t size;
	int rv;

	bitmap = cbfs_map(argv[2], &size);
	if (!bitmap) {
		console_printf("File '%s' not found\n", argv[2]);
		return -1;
	}
	const struct scale pos_rel = {
		.x = { .n = strtoul(argv[3], NULL, 0), .d = 100, },
		.y = { .n = strtoul(argv[4], NULL, 0), .d = 100, },
	};

	if (argc == 7) {
		const struct scale s = {
			.x = { .n = strtoul(argv[5], NULL, 0), .d = 100, },
			.y = { .n = strtoul(argv[6], NULL, 0), .d = 100, },
		};
		dim_rel = &s;
	}

	rv = draw_bitmap(bitmap, size, &pos_rel, dim_rel,
			   PIVOT_H_LEFT|PIVOT_V_TOP);
	if (rv)
		console_printf("draw_bitmap returned error: %d\n", rv);

	free(bitmap);

	return rv;
}

static int do_print_info(void)
{
	struct cb_framebuffer *fbinfo = &lib_sysinfo.framebuffer;
	if (!fbinfo) {
		console_printf("Invalid address for framebuffer info\n");
		return -1;
	}

	unsigned char *fbaddr =
		(unsigned char *)(uintptr_t)(fbinfo->physical_address);

	console_printf("physical_address=0x%p\n", fbaddr);
	console_printf("x_resolution=%d\n", fbinfo->x_resolution);
	console_printf("y_resolution=%d\n", fbinfo->y_resolution);
	console_printf("bytes_per_line=%d\n", fbinfo->bytes_per_line);
	console_printf("bits_per_pixel=%d\n", fbinfo->bits_per_pixel);
	console_printf("red_mask_pos=%d\n", fbinfo->red_mask_pos);
	console_printf("red_mask_size=%d\n", fbinfo->red_mask_size);
	console_printf("green_mask_pos=%d\n", fbinfo->green_mask_pos);
	console_printf("green_mask_size=%d\n", fbinfo->green_mask_size);
	console_printf("blue_mask_pos=%d\n", fbinfo->blue_mask_pos);
	console_printf("blue_mask_size=%d\n", fbinfo->blue_mask_size);

	return 0;
}

static int do_draw_box(char * const argv[])
{
	const struct rect box = {
		.offset = {
			.x = strtoul(argv[2], NULL, 0),
			.y = strtoul(argv[3], NULL, 0),
		},
		.size = {
			.width = strtoul(argv[4], NULL, 0),
			.height = strtoul(argv[5], NULL, 0),
		},
	};
	const struct rgb_color rgb = {
			.red = strtoul(argv[6], NULL, 0),
			.green = strtoul(argv[7], NULL, 0),
			.blue = strtoul(argv[8], NULL, 0),
	};
	return draw_box(&box, &rgb);
}

static int do_draw(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int rv = CMD_RET_SUCCESS;

	if (init_display()) {
		console_printf("Failed to initialize display\n");
		return CMD_RET_FAILURE;
	}

	if ((argc == 3 || argc == 7) && !strcmp("image", argv[1])) {
		if (do_draw_image(argc, argv))
			rv = CMD_RET_FAILURE;
	} else if (argc == 2 && !strcmp("info", argv[1])) {
		if (do_print_info())
			rv = CMD_RET_FAILURE;
	} else if (argc == 9 && !strcmp("box", argv[1])) {
		if (do_draw_box(argv))
			rv = CMD_RET_FAILURE;
	} else {
		console_printf("Syntax error\n");
		rv = CMD_RET_USAGE;
	}

	return rv;
}

U_BOOT_CMD(
	   draw,	9,	1,
	   "draw image on screen, print screen info, etc.",
	   "image <name> <x> <y> [<width> <height>] - draw image in cbfs at (x, y)\n"
	   "info - print framebuffer information\n"
	   "box <x> <y> <width> <height> <red> <green> <blue> - draw a box\n"
);
