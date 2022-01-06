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
	if (strlen(__LOG__))
		VB2_TRY(ui_log_init(__SCREEN__, "en", __LOG__,
				    &global_ui_log_info));
	else if (strlen(__LOG_FILE__)) {
		size_t read_count = read_file(
			log_buffer, sizeof(log_buffer) - 1, __LOG_FILE__);
		if (read_count == sizeof(log_buffer) - 1) {
			printf("WARN: Log file may larger than %zu bytes. Only "
			       "the first %zu bytes are read.\n",
			       sizeof(log_buffer) - 1, read_count);
		}
		log_buffer[read_count] = '\0';
		VB2_TRY(ui_log_init(__SCREEN__, "en", log_buffer,
				    &global_ui_log_info));
	}

	VB2_TRY(ui_display(__SCREEN__, 0, __ITEM__, __DISABLED_ITEM_MASK__,
			   __HIDDEN_ITEM_MASK__, 0, __PAGE__, 0));

	if (argc < 2) {
		printf("ERROR: No output path specified\n");
		return -1;
	}

	size_t size = sizeof(framebuffer);
	if (write_file(framebuffer, size, argv[1]) != size)
		return -1;
	return 0;
}
