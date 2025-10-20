/*
 * Copyright 2012 Google LLC
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

#ifndef __BOOT_COMMANDLINE_H__
#define __BOOT_COMMANDLINE_H__

#include "boot/commandline.h"
#include <stddef.h>
#include <stdint.h>

#include "base/gpt.h"

struct commandline_info {
	int devnum;
	int partnum;
	Guid *guid;
	int external_gpt;
};

int commandline_subst(const char *src, char *dest, size_t dest_size,
		      const struct commandline_info *info);

/* Append a command line parameter to the end of the command line. */
void commandline_append(const char *param);


/*
 * Get the unsigned integer value of a command line parameter.
 *
 * @param cmdline	The command line string to parse.
 * @param key		The key of the parameter to find.
 * @param out		Pointer to a uint32_t to store the value.
 * @return		0 on success, -1 if not found or invalid.
 */
int get_cmdline_uint32_value(const char *cmdline, const char *key, uint32_t *out);

#endif /* __BOOT_COMMAND_LINE_H__ */
