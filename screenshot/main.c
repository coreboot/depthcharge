// SPDX-License-Identifier: GPL-2.0

#include <stdio.h>
#include <sysinfo.h>
#include <vb2_api.h>

#include "io.h"
#include "vboot/ui.h"

#ifndef STRINGIFY
#define _STRINGIFY(x) #x
#define STRINGIFY(x) _STRINGIFY(x)
#endif

#define BYTES_PER_PIXEL 3

static unsigned char framebuffer[__HEIGHT__ * __WIDTH__ * BYTES_PER_PIXEL];

struct sysinfo_t lib_sysinfo = {
	.framebuffer = {
		.y_resolution = __HEIGHT__,
		.x_resolution = __WIDTH__,
		.bits_per_pixel = BYTES_PER_PIXEL * 8,
		.bytes_per_line = BYTES_PER_PIXEL * __WIDTH__,
		.physical_address = (uintptr_t)framebuffer,
		.red_mask_pos = 0,
		.red_mask_size = 8,
		.green_mask_pos = 8,
		.green_mask_size = 8,
		.blue_mask_pos = 16,
		.blue_mask_size = 8,
	},
};

int main(int argc, char *argv[])
{
	static struct ui_log_info log;
	const struct ui_screen_info *screen_info;
	struct ui_locale locale = {
		.code = "en",
		.rtl = 0,
	};
	screen_info = ui_get_screen_info(__SCREEN__);
	printf("Generating screenshot: %dx%d\n", __WIDTH__, __HEIGHT__);
	printf("- screen: %s (%#x)\n", STRINGIFY(__SCREEN__), __SCREEN__);
	printf("- selected_item: %d\n", __ITEM__);
	printf("- disabled_item_mask: %#x\n", __DISABLED_ITEM_MASK__);
	printf("- hidden_item_mask: %#x\n", __HIDDEN_ITEM_MASK__);
	if (!screen_info) {
		printf("ERROR: screen_info is null\n");
		return -1;
	}
	struct ui_state state = {
		.screen = screen_info,
		.locale = &locale,
		.selected_item = __ITEM__,
		.disabled_item_mask = __DISABLED_ITEM_MASK__,
		.hidden_item_mask = __HIDDEN_ITEM_MASK__,
		.timer_disabled = 0,
		.log = &log,
		.current_page = 0,
		.error_code = 0,
	};
	if (strlen(__LOG__))
		VB2_TRY(ui_log_init(__SCREEN__, locale.code, __LOG__, &log));
	VB2_TRY(ui_display_screen(&state, NULL));
	flush_graphics_buffer();
	if (argc < 2) {
		printf("ERROR: No output path specified\n");
		return -1;
	}

	size_t size = sizeof(framebuffer);
	if (write_raw(framebuffer, size, argv[1]) != size)
		return -1;
	return 0;
}
