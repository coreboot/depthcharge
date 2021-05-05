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

#ifndef __FASTBOOT_FASTBOOT_H__
#define __FASTBOOT_FASTBOOT_H__

#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>

//#define NDEBUG 0

#ifndef NDEBUG
#define FB_DEBUG(...)                                                          \
	do {                                                                   \
		printf("%s: ", __func__);                                      \
		printf(__VA_ARGS__);                                           \
	} while (0)
#else
#define FB_DEBUG(...)
#endif

#if 0
// This is _very_ slow when transferring data.
#define FB_TRACE_IO(...) printf(__VA_ARGS__)
#else
#define FB_TRACE_IO(...)
#endif

#define FASTBOOT_MSG_MAX 64
#define FASTBOOT_MAX_DOWNLOAD_SIZE ((uint64_t)CONFIG_KERNEL_SIZE)

enum fastboot_state {
	// Expecting a command. This is the initial state.
	COMMAND = 0,
	// Expecting data.
	DOWNLOAD,
	// Session finished, resume normal boot.
	FINISHED,
	// Session finished, reboot.
	REBOOT,
};

// State of a fastboot session.
typedef struct fastboot_session {
	// State of the session.
	enum fastboot_state state;

	// Is there a download present?
	bool has_download;
	// If has_download, how much data is there?
	// If state == DOWNLOAD, how much data are we expecting?
	uint64_t download_len;
	// If state == DOWNLOAD, how much data have we received?
	uint64_t download_progress;
} fastboot_session_t;

// Have we exited fastboot? (e.g. user ran `fastboot continue`)
bool fastboot_is_finished(fastboot_session_t *fb);
// Handle a single fastboot packet, or a chunk of data in a download.
void fastboot_handle_packet(fastboot_session_t *fb, void *data, uint64_t len);
// Run fastboot.
void fastboot(void);
// Get the download buffer and the length of the download if a download exists.
void *fastboot_get_download_buffer(fastboot_session_t *fb, uint64_t *len);

// Responses to the client.
void fastboot_fail(fastboot_session_t *fb, const char *msg);
void fastboot_data(fastboot_session_t *fb, uint32_t bytes);
void fastboot_okay(fastboot_session_t *fb, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));
void fastboot_info(fastboot_session_t *fb, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));
// Sends an empty OKAY.
void fastboot_succeed(fastboot_session_t *fb);

#endif // __FASTBOOT_FASTBOOT_H__
