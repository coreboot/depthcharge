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

#include "fastboot/cmd.h"
#include "fastboot/fastboot.h"
#include "fastboot/vars.h"
#include "gpt_misc.h"
#include <stdlib.h>

static int parse_hex(const char *str, size_t len, uint32_t *ret)
{
	int valid = 0;
	uint32_t result = 0;
	for (size_t i = 0; i < len; i++) {
		char c = str[i];
		int units = 0;
		if (c >= '0' && c <= '9') {
			units = c - '0';
		} else if (c >= 'A' && c <= 'F') {
			units = 10 + (c - 'A');
		} else if (c >= 'a' && c <= 'f') {
			units = 10 + (c - 'a');
		} else {
			break;
		}

		result *= 0x10;
		result += units;
		valid++;
	}

	*ret = result;

	return valid;
}

static void fastboot_cmd_continue(fastboot_session_t *fb, const char *arg,
				  uint64_t arg_len)
{
	fastboot_okay(fb, "Continuing boot");
	fb->state = FINISHED;
}

static void fastboot_cmd_download(fastboot_session_t *fb, const char *arg,
				  uint64_t arg_len)
{
	uint32_t size = 0;
	int digits = parse_hex(arg, arg_len, &size);
	if (digits != arg_len) {
		fastboot_fail(fb, "Invalid argument");
		return;
	}

	if (size > FASTBOOT_MAX_DOWNLOAD_SIZE) {
		fastboot_fail(fb, "File too big");
		return;
	}

	fastboot_data(fb, size);
}

static void fastboot_cmd_reboot(fastboot_session_t *fb, const char *arg,
				uint64_t arg_len)
{
	fastboot_succeed(fb);
	fb->state = REBOOT;
}

#define CMD_ARGS(_name, _sep, _fn)                                             \
	{                                                                      \
		.name = _name, .has_args = true, .sep = _sep, .fn = _fn        \
	}
#define CMD_NO_ARGS(_name, _fn)                                                \
	{                                                                      \
		.name = _name, .has_args = false, .fn = _fn                    \
	}
struct fastboot_cmd fastboot_cmds[] = {
	CMD_NO_ARGS("continue", fastboot_cmd_continue),
	CMD_ARGS("download", ':', fastboot_cmd_download),
	CMD_ARGS("getvar", ':', fastboot_cmd_getvar),
	CMD_NO_ARGS("reboot", fastboot_cmd_reboot),
	{
		.name = NULL,
	},
};
