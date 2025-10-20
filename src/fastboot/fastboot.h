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

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#include "gpt_misc.h"
#include "debug/dev.h"
#include "drivers/storage/blockdev.h"

#define FB_DEBUG(...)                                                          \
	do {                                                                   \
		printf("%s: ", __func__);                                      \
		printf(__VA_ARGS__);                                           \
	} while (0)

#if 0
/* This is _very_ slow when transferring data */
#define FB_TRACE_IO(...) printf(__VA_ARGS__)
#else
#define FB_TRACE_IO(...)
#endif

#define FASTBOOT_MSG_MAX 256
#define FASTBOOT_PREFIX_LEN 4
#define FASTBOOT_MSG_LEN_WO_PREFIX (FASTBOOT_MSG_MAX - FASTBOOT_PREFIX_LEN)
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

enum fastboot_connection_type {
	FASTBOOT_TCP_CONN,
	FASTBOOT_USB_CONN,
};

/* State of a fastboot session and functions that abstract transport layer */
struct FastbootOps {
	/* State of the session */
	enum fastboot_state state;

	/* Device used as a target for flash/erase command. Maybe NULL. */
	BlockDev *disk;
	/* GPT of the disk, maybe NULL */
	GptData *gpt;

	/* fastboot_get_memory_buffer() has staged data */
	bool has_staged_data;
	/*
	 * Number of bytes in the memory buffer (or expected number of bytes after
	 * transfer completes when in DOWNLOAD state)
	 */
	uint64_t memory_buffer_len;
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

	/* Type of transport layer */
	enum fastboot_connection_type type;
	/* Serial number of connection */
	char *serial;
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
/* Get the buffer for upload/download and the length of the data in the buffer */
void *fastboot_get_memory_buffer(struct FastbootOps *fb, uint64_t *len);
/* Prepares the staging buffer for download */
void fastboot_prepare_download(struct FastbootOps *fb, uint32_t bytes);
/* Reset fastboot session to initial state */
void fastboot_reset_session(struct FastbootOps *fb);
/* Resets the state of the data staging area */
void fastboot_reset_staging(struct FastbootOps *fb);

/* Responses to the client */
void fastboot_send_fmt(struct FastbootOps *fb, enum fastboot_response_type t, int print_msg,
		       const char *fmt, ...) __attribute__((format(printf, 4, 5)));

#define fastboot_okay(fb, fmt, ...) \
	fastboot_send_fmt((fb), FASTBOOT_RES_OKAY, 1, (fmt), ##__VA_ARGS__)

#define fastboot_fail(fb, fmt, ...) \
	fastboot_send_fmt((fb), FASTBOOT_RES_FAIL, 1, (fmt), ##__VA_ARGS__)

#define fastboot_info(fb, fmt, ...) \
	fastboot_send_fmt((fb), FASTBOOT_RES_INFO, 1, (fmt), ##__VA_ARGS__)

#define fastboot_text(fb, fmt, ...) \
	fastboot_send_fmt((fb), FASTBOOT_RES_TEXT, 1, (fmt), ##__VA_ARGS__)

/* Sends an empty OKAY */
#define fastboot_succeed(fb) fastboot_send_fmt((fb), FASTBOOT_RES_OKAY, 1, "")

/* Sends DATA with the length */
#define fastboot_data(fb, len) fastboot_send_fmt((fb), FASTBOOT_RES_DATA, 1, "%08x", (len))

#endif /* __FASTBOOT_FASTBOOT_H__ */
