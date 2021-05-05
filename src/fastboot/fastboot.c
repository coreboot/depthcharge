/*
 * Copyright (C) 2021 Google Inc.
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

#include "fastboot/fastboot.h"

#include <stdlib.h>
#include "die.h"
#include "fastboot/cmd.h"
#include "fastboot/tcp.h"
#include "image/symbols.h"
#include "stdarg.h"

static char msg_buf[FASTBOOT_MSG_MAX];
void fastboot_fail(fastboot_session_t *fb, const char *msg)
{
	int len = snprintf(msg_buf, FASTBOOT_MSG_MAX, "FAIL%s", msg);
	fastboot_send(msg_buf, len + 1);
}

void fastboot_data(fastboot_session_t *fb, uint32_t bytes)
{
	// Set up for data transfer.
	fb->download_len = bytes;
	fb->download_progress = 0;
	fb->has_download = false;
	fb->state = DOWNLOAD;
	// Send acknowledgement to host.
	int len = snprintf(msg_buf, FASTBOOT_MSG_MAX, "DATA%08x", bytes);
	fastboot_send(msg_buf, len + 1);
}

void fastboot_okay(fastboot_session_t *fb, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	memcpy(msg_buf, "OKAY", 4);
	int len = 4 + vsnprintf(&msg_buf[4], FASTBOOT_MSG_MAX - 4, fmt, ap);
	va_end(ap);
	fastboot_send(msg_buf, len + 1);
}

void fastboot_info(fastboot_session_t *fb, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	memcpy(msg_buf, "INFO", 4);
	int len = 4 + vsnprintf(&msg_buf[4], FASTBOOT_MSG_MAX - 4, fmt, ap);
	va_end(ap);
	fastboot_send(msg_buf, len + 1);
}

void fastboot_succeed(fastboot_session_t *fb) { fastboot_send("OKAY", 5); }

bool fastboot_is_finished(fastboot_session_t *fb)
{
	return fb->state == FINISHED || fb->state == REBOOT;
}

/***************************** PROTOCOL HANDLING *****************************/

static void fastboot_handle_command(fastboot_session_t *fb, void *data,
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

void fastboot_handle_packet(fastboot_session_t *fb, void *data, uint64_t len)
{
	switch (fb->state) {
	case COMMAND:
		fastboot_handle_command(fb, data, len);
		break;
	case DOWNLOAD:
	case FINISHED:
	case REBOOT:
		break;
	default:
		die("Unknown state %d\n", fb->state);
	}
}

void fastboot(void) { fastboot_over_tcp(); }
