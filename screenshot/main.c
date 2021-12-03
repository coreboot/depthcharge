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
#define LOG_BUFFER_LEN 65536

static unsigned char framebuffer[__HEIGHT__ * __WIDTH__ * BYTES_PER_PIXEL];
static char log_buffer[LOG_BUFFER_LEN];

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
	struct vb2_context fake_ctx;
	struct ui_context ui;
	struct ui_state *state;

	memset(&fake_ctx, 0, sizeof(fake_ctx));
	vb2api_set_locale_id(&fake_ctx, 0);
	VB2_TRY(ui_init_context(&ui, &fake_ctx, __SCREEN__));
	state = ui.state;
	state->selected_item = __ITEM__;
	state->disabled_item_mask = __DISABLED_ITEM_MASK__;
	state->hidden_item_mask = __HIDDEN_ITEM_MASK__;
	state->current_page = __PAGE__;

	/*
	 * ui_log_init() will be called once from ui_init_context(). Another
	 * call below will replace the mock log string with the provided one.
	 */
	if (strlen(__LOG__)) {
		VB2_TRY(ui_log_init(__SCREEN__, "en", __LOG__, &state->log));
	} else if (strlen(__LOG_FILE__)) {
		size_t read_count = read_file(
			log_buffer, sizeof(log_buffer) - 1, __LOG_FILE__);
		if (read_count == sizeof(log_buffer) - 1) {
			printf("WARN: Log file may larger than %zu bytes. Only "
			       "the first %zu bytes are read.\n",
			       sizeof(log_buffer) - 1, read_count);
		}
		log_buffer[read_count] = '\0';
		VB2_TRY(ui_log_init(__SCREEN__, "en", log_buffer, &state->log));
	}

	VB2_TRY(ui_display(&ui, NULL));

	if (argc < 2) {
		printf("ERROR: No output path specified\n");
		return -1;
	}

	size_t size = sizeof(framebuffer);
	if (write_file(framebuffer, size, argv[1]) != size)
		return -1;
	return 0;
}
