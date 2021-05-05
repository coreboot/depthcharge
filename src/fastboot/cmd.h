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

#ifndef __FASTBOOT_CMD_H__
#define __FASTBOOT_CMD_H__

#include "fastboot/fastboot.h"
#include <stdio.h>
#include <stdint.h>

typedef void (*fastboot_cmd_fn_t)(fastboot_session_t *fb, const char *arg,
				  uint64_t arg_len);

// Represents a command.
typedef struct fastboot_cmd {
	// Name of the command.
	const char *name;
	// Does this command take arguments?
	bool has_args;
	// Separator between arguments if has_args.
	// E.g. has_args is true, and sep is ':', we would expect to see:
	// "<name>:<arguments>", and fn would be called like `fn(session,
	// "<arguments>", strlen("<arguments>"))`.
	const char sep;
	// Function to call. Arg will be the start of the first argument (not
	// its separator). For instance: download:0x800 => fn("0x800",
	// strlen("0x800")). Arguments might not be null-terminated.
	fastboot_cmd_fn_t fn;
} fastboot_cmd_t;

extern struct fastboot_cmd fastboot_cmds[];

#endif // __FASTBOOT_CMD_H__
