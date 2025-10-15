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
#include "drivers/ec/cros/ec.h"
#include "fastboot/cmd.h"
#include "fastboot/fastboot.h"
#include "fastboot/tcp.h"
#include "fastboot/usb.h"
#include "image/symbols.h"
#include "stdarg.h"

void fastboot_send_fmt(struct FastbootOps *fb, enum fastboot_response_type t,
		       const char *fmt, ...)
{
	static const char response_prefix[][FASTBOOT_PREFIX_LEN] = {
		[FASTBOOT_RES_OKAY] = "OKAY",
		[FASTBOOT_RES_FAIL] = "FAIL",
		[FASTBOOT_RES_DATA] = "DATA",
		[FASTBOOT_RES_INFO] = "INFO",
		[FASTBOOT_RES_TEXT] = "TEXT",
	};
	char msg_buf[FASTBOOT_MSG_MAX];
	va_list ap;
	va_start(ap, fmt);
	memcpy(msg_buf, response_prefix[t], FASTBOOT_PREFIX_LEN);
	int len = FASTBOOT_PREFIX_LEN + vsnprintf(&msg_buf[FASTBOOT_PREFIX_LEN],
						  FASTBOOT_MSG_LEN_WO_PREFIX, fmt, ap);
	va_end(ap);
	if (len < FASTBOOT_PREFIX_LEN) {
		/* For some reason vsnprintf failed, send response prefix anyway */
		printf("fastboot send vsnprintf failed (%d)\n", len - FASTBOOT_PREFIX_LEN);
		len = FASTBOOT_PREFIX_LEN;
		/* Make sure that message is ended with NULL char */
		msg_buf[len] = '\0';
	} else if (len >= FASTBOOT_MSG_MAX) {
		printf("fastboot send message truncated\n");
		/* vsnprintf sets msg_buf[FASTBOOT_MSG_MAX - 1] to '\0' in that case */
		len = FASTBOOT_MSG_MAX - 1;
	}
	printf("FB send %.4s: %s\n", msg_buf, msg_buf + 4);
	fb->send_packet(fb, msg_buf, len + 1);
}

void fastboot_prepare_download(struct FastbootOps *fb, uint32_t bytes) {
	fb->memory_buffer_len = bytes;
	fb->download_progress = 0;
	fb->has_staged_data = false;
	fb->state = DOWNLOAD;
}

bool fastboot_is_finished(struct FastbootOps *fb)
{
	return fb->state == FINISHED || fb->state == REBOOT;
}

void *fastboot_get_memory_buffer(struct FastbootOps *fb, uint64_t *len)
{
	if (len)
		*len = fb->memory_buffer_len;
	return &_kernel_start;
}

/***************************** PROTOCOL HANDLING *****************************/

static void fastboot_handle_download(struct FastbootOps *fb, void *data,
				     uint64_t len)
{
	uint64_t left = fb->memory_buffer_len - fb->download_progress;
	if (len > left) {
		fastboot_fail(fb, "Too much data");
		fb->state = COMMAND;
		return;
	}

	void *buf = fastboot_get_memory_buffer(fb, NULL);
	memcpy(buf + fb->download_progress, data, len);
	fb->download_progress += len;
	if (len == left) {
		fb->has_staged_data = true;
		fb->download_progress = 0;
		fb->state = COMMAND;
		fastboot_succeed(fb);
	}
}

static void fastboot_handle_command(struct FastbootOps *fb, void *data,
				    uint64_t len)
{
	// "data" contains a command, which is essentially freeform text.
	char *cmd = (char *)data;
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
		/* Move arguments to the beginning of the buffer */
		memmove(data, cmd, len);
		((char *)data)[len] = '\0';

		cur->fn(fb, data);
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
	fb->download_progress = 0;

	/* Reset transport layer specific data */
	if (fb->reset)
		fb->reset(fb);
}

void fastboot_reset_staging(struct FastbootOps *fb)
{
	fb->has_staged_data = false;
	fb->memory_buffer_len = 0;
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

	if (CONFIG(FASTBOOT_USB_ALINK)) {
		video_console_set_cursor(0, 1);
		video_printf(0, 0, VIDEO_PRINTF_ALIGN_LEFT, "Wait for USB");
		fb_session = fastboot_setup_usb();
	}

	if (CONFIG(FASTBOOT_TCP) && fb_session == NULL) {
		video_console_set_cursor(0, 1);
		video_printf(0, 0, VIDEO_PRINTF_ALIGN_LEFT, "Wait for network");
		fb_session = fastboot_setup_tcp();
	}

	if (fb_session == NULL) {
		video_console_set_cursor(0, 1);
		video_printf(0, 0, VIDEO_PRINTF_ALIGN_LEFT, "No fastboot device");
		return;
	}

	video_console_set_cursor(0, 1);
	switch (fb_session->type) {
	case FASTBOOT_TCP_CONN:
		video_printf(0, 0, VIDEO_PRINTF_ALIGN_LEFT, "Network connected");
		break;
	case FASTBOOT_USB_CONN:
		video_printf(0, 0, VIDEO_PRINTF_ALIGN_LEFT, "USB connected");
		break;
	}
	video_console_set_cursor(0, 2);
	video_printf(0, 0, VIDEO_PRINTF_ALIGN_LEFT, fb_session->serial);

	if (CONFIG(DRIVER_EC_CROS))
		cros_ec_print("Fastboot %s\n", fb_session->serial);

	fastboot_reset_staging(fb_session);
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
