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

#ifndef __FASTBOOT_FASTBOOT_H__
#define __FASTBOOT_FASTBOOT_H__

#include <libpayload.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

//#define NDEBUG 0

#ifndef NDEBUG
#define FB_DEBUG(...)                                                          \
	do {                                                                   \
		printf("%s: ", __func__);                                      \
		printf(__VA_ARGS__);                                           \
	} while (0)

#define FB_DEBUG_LINE(...)             \
	do {                           \
		FB_DEBUG(__VA_ARGS__); \
		putchar('\n');         \
	} while (0)
#else
#define FB_DEBUG(...)
#define FB_DEBUG_LINE(...)
#endif

#if 0
/* This is _very_ slow when transferring data */
#define FB_TRACE_IO(...) printf(__VA_ARGS__)
#else
#define FB_TRACE_IO(...)
#endif

/* Call fastboot_fail if fb is not NULL and print the same message using FB_DEBUG */
#define FB_FAIL_AND_DEBUG(fb, ...)                      \
	do {                                            \
		if (fb)                                 \
			fastboot_fail(fb, __VA_ARGS__); \
		FB_DEBUG_LINE(__VA_ARGS__);             \
	} while(0)

#define FASTBOOT_MSG_MAX 64
#define FASTBOOT_MAX_DOWNLOAD_SIZE ((uint64_t)CONFIG_KERNEL_SIZE)
/* Maximum length of command packet as stated in the Fastboot documentation */
#define FASTBOOT_COMMAND_MAX 4096

enum fastboot_state {
	/* Expecting a command. This is the initial state. */
	COMMAND = 0,
	/* Expecting data */
	DOWNLOAD,
	/* Session finished, resume normal boot */
	FINISHED,
	/* Session finished, reboot */
	REBOOT,
};

/* State of a fastboot session and functions that abstract transport layer */
struct FastbootOps {
	/* State of the session */
	enum fastboot_state state;

	/* Download buffer is ready to obtain from fastboot_get_download_buffer() */
	bool has_download;
	/*
	 * Number of bytes in download buffer (or expected number of bytes after transfer
	 * completes when in DOWNLOAD state)
	 */
	uint64_t download_len;
	/* Actual number of bytes received when in DOWNLOAD state */
	uint64_t download_progress;
	/*
	 * Poll for new fastboot messages. This function should call
	 * fastboot_handle_packet(). This function is required.
	 */
	void (*poll)(struct FastbootOps *fb);
	/* Send fastboot response. This function is required. */
	int (*send_packet)(struct FastbootOps *fb, void *buf, size_t len);
	/* Called on fastboot_reset_session(). This function is optional. */
	void (*reset)(struct FastbootOps *fb);
	/*
	 * Called on fastboot exit. After this function, any function from
	 * fastboot_transport shouldn't be called. This function is optional.
	 */
	void (*release)(struct FastbootOps *fb);
};

enum fastboot_response_type {
	FASTBOOT_RES_OKAY = 0,
	FASTBOOT_RES_FAIL,
	FASTBOOT_RES_DATA,
	FASTBOOT_RES_INFO,
	FASTBOOT_RES_TEXT,
};

/* Have we exited fastboot? (e.g. user ran `fastboot continue)` */
bool fastboot_is_finished(struct FastbootOps *fb);
/* Handle a single fastboot packet, or a chunk of data in a download */
void fastboot_handle_packet(struct FastbootOps *fb, void *data, uint64_t len);
/* Run fastboot */
void fastboot(void);
/* Get the download buffer and the length of the download if a download exists */
void *fastboot_get_download_buffer(struct FastbootOps *fb, uint64_t *len);
/* Reset fastboot session to initial state */
void fastboot_reset_session(struct FastbootOps *fb);

/* Responses to the client */
void fastboot_send_fmt(struct FastbootOps *fb, enum fastboot_response_type t,
		       const char *fmt, ...) __attribute__((format(printf, 3, 4)));

#define fastboot_okay(fb, fmt, ...) \
	fastboot_send_fmt((fb), FASTBOOT_RES_OKAY, (fmt), ##__VA_ARGS__)

#define fastboot_fail(fb, fmt, ...) \
	fastboot_send_fmt((fb), FASTBOOT_RES_FAIL, (fmt), ##__VA_ARGS__)

#define fastboot_info(fb, fmt, ...) \
	fastboot_send_fmt((fb), FASTBOOT_RES_INFO, (fmt), ##__VA_ARGS__)

#define fastboot_text(fb, fmt, ...) \
	fastboot_send_fmt((fb), FASTBOOT_RES_TEXT, (fmt), ##__VA_ARGS__)

// Sends an empty OKAY.
#define fastboot_succeed(fb) fastboot_send_fmt((fb), FASTBOOT_RES_OKAY, "%s", "")

void fastboot_data(struct FastbootOps *fb, uint32_t bytes);

#endif /* __FASTBOOT_FASTBOOT_H__ */
