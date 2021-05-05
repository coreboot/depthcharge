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
#include "gpt_misc.h"
#include <stdlib.h>

static void fastboot_cmd_continue(fastboot_session_t *fb, const char *arg,
				  uint64_t arg_len)
{
	fastboot_okay(fb, "Continuing boot");
	fb->state = FINISHED;
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
	CMD_NO_ARGS("reboot", fastboot_cmd_reboot),
	{
		.name = NULL,
	},
};
