/*
 * Copyright 2021 Google LLC
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <libpayload.h>
#include <stdlib.h>

#include "die.h"
#include "fastboot/cmd.h"
#include "fastboot/fastboot.h"
#include "fastboot/tcp.h"
#include "image/symbols.h"
#include "stdarg.h"

static char msg_buf[FASTBOOT_MSG_MAX];
void fastboot_fail(struct FastbootOps *fb, const char *msg)
{
	int len = snprintf(msg_buf, FASTBOOT_MSG_MAX, "FAIL%s", msg);
	fb->send_packet(fb, msg_buf, len + 1);
}

void fastboot_data(struct FastbootOps *fb, uint32_t bytes)
{
	/* Set up for data transfer */
	fb->download_len = bytes;
	fb->download_progress = 0;
	fb->has_download = false;
	fb->state = DOWNLOAD;
	/* Send acknowledgment to host */
	int len = snprintf(msg_buf, FASTBOOT_MSG_MAX, "DATA%08x", bytes);
	fb->send_packet(fb, msg_buf, len + 1);
}

void fastboot_okay(struct FastbootOps *fb, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	memcpy(msg_buf, "OKAY", 4);
	int len = 4 + vsnprintf(&msg_buf[4], FASTBOOT_MSG_MAX - 4, fmt, ap);
	va_end(ap);
	fb->send_packet(fb, msg_buf, len + 1);
}

void fastboot_info(struct FastbootOps *fb, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	memcpy(msg_buf, "INFO", 4);
	int len = 4 + vsnprintf(&msg_buf[4], FASTBOOT_MSG_MAX - 4, fmt, ap);
	va_end(ap);
	fb->send_packet(fb, msg_buf, len + 1);
}

void fastboot_succeed(struct FastbootOps *fb)
{
	fb->send_packet(fb, "OKAY", 5);
}

bool fastboot_is_finished(struct FastbootOps *fb)
{
	return fb->state == FINISHED || fb->state == REBOOT;
}

void *fastboot_get_download_buffer(struct FastbootOps *fb, uint64_t *len)
{
	if (len)
		*len = fb->download_len;
	return &_kernel_start;
}

/***************************** PROTOCOL HANDLING *****************************/

static void fastboot_handle_download(struct FastbootOps *fb, void *data,
				     uint64_t len)
{
	uint64_t left = fb->download_len - fb->download_progress;
	if (len > left) {
		fastboot_fail(fb, "Too much data");
		fb->state = COMMAND;
		return;
	}

	void *buf = fastboot_get_download_buffer(fb, NULL);
	memcpy(buf + fb->download_progress, data, len);
	fb->download_progress += len;
	if (len == left) {
		fb->has_download = true;
		fb->download_progress = 0;
		fb->state = COMMAND;
		fastboot_succeed(fb);
	}
}

static void fastboot_handle_command(struct FastbootOps *fb, void *data,
				    uint64_t len)
{
	// "data" contains a command, which is essentially freeform text.
	const char *cmd = (const char *)data;
	for (int i = 0; fastboot_cmds[i].name != NULL; i++) {
		fastboot_cmd_t *cur = &fastboot_cmds[i];
		// See if the command's name matches.
		int name_strlen = strlen(cur->name);
		if (name_strlen > len)
			continue;
		if (strncmp(cur->name, cmd, name_strlen))
			continue;

		// Name matched - move past the name, and check arguments (if
		// any are expected). At this point the string will start with
		// the separator (e.g. "flash:kern-a" would match the "flash"
		// command above, and so after this we have ":kern-a" left in
		// `cmd`).
		cmd += name_strlen;
		len -= name_strlen;

		if (cur->has_args) {
			if (len < 1 || cmd[0] != cur->sep) {
				// Command expected arguments, but has none.
				// Try and see if there is a better match.
				cmd -= name_strlen;
				len += name_strlen;
				continue;
			}
			// Move past the separator, so that the command doesn't
			// see a leading separator in its arguments. This takes
			// ":kern-a" from above and makes it just "kern-a".
			cmd++;
			len--;
		}

		cur->fn(fb, cmd, len);
		return;
	}
	fastboot_fail(fb, "Unknown command");
}

void fastboot_handle_packet(struct FastbootOps *fb, void *data, uint64_t len)
{
	switch (fb->state) {
	case COMMAND:
		fastboot_handle_command(fb, data, len);
		break;
	case DOWNLOAD:
		fastboot_handle_download(fb, data, len);
		break;
	case FINISHED:
	case REBOOT:
		break;
	default:
		die("Unknown state %d\n", fb->state);
	}
}

void fastboot_reset_session(struct FastbootOps *fb)
{
	/* Reset common fastboot session */
	fb->state = COMMAND;
	fb->has_download = false;

	/* Reset transport layer specific data */
	if (fb->reset)
		fb->reset(fb);
}

void fastboot(void)
{
	struct FastbootOps *fb_session = NULL;
	enum fastboot_state final_state;

	/* TODO(b/370988331): Replace this with actual UI */
	video_init();
	video_console_clear();
	/* Print red "Fastboot" on the top */
	video_console_set_cursor(0, 0);
	video_printf(1, 0, VIDEO_PRINTF_ALIGN_LEFT, "Fastboot\n");

	if (CONFIG(FASTBOOT_TCP)) {
		video_console_set_cursor(0, 1);
		video_printf(0, 0, VIDEO_PRINTF_ALIGN_LEFT, "Wait for network");
		fb_session = fastboot_setup_tcp();
		if (fb_session) {
			video_console_set_cursor(0, 1);
			video_printf(0, 0, VIDEO_PRINTF_ALIGN_LEFT, "Network connected");
		}
	}

	if (fb_session == NULL) {
		video_console_set_cursor(0, 1);
		video_printf(0, 0, VIDEO_PRINTF_ALIGN_LEFT, "No fastboot device");
		return;
	}

	fastboot_reset_session(fb_session);

	printf("fastboot starting.\n");
	while (!fastboot_is_finished(fb_session))
		fb_session->poll(fb_session);

	printf("fastboot done.\n");
	final_state = fb_session->state;
	if (fb_session->release)
		fb_session->release(fb_session);

	if (final_state == REBOOT)
		reboot();
}
