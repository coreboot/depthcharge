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

#ifndef __FASTBOOT_VARS_H__
#define __FASTBOOT_VARS_H__

#include "fastboot/fastboot.h"
#include <stdbool.h>
#include <stdio.h>

typedef enum fastboot_var {
	VAR_CURRENT_SLOT,
	VAR_DOWNLOAD_SIZE,
	VAR_IS_USERSPACE,
	VAR_PARTITION_SIZE,
	VAR_PRODUCT,
	VAR_SECURE,
	VAR_SLOT_COUNT,
	VAR_VERSION,
} fastboot_var_t;

typedef enum fastboot_getvar_result {
	STATE_OK,
	STATE_UNKNOWN_VAR,
	STATE_DISK_ERROR,
	STATE_TRY_NEXT,
	STATE_LAST,
} fastboot_getvar_result_t;

typedef struct fastboot_getvar_info {
	const char *name;
	bool has_args;
	char sep;
	fastboot_var_t var;
} fastboot_getvar_info_t;

void fastboot_cmd_getvar(struct FastbootOps *fb, const char *args);
fastboot_getvar_result_t fastboot_getvar(struct FastbootOps *fb, fastboot_var_t var,
					 const char *arg, size_t index, char *outbuf,
					 size_t *outbuf_len);
#endif // __FASTBOOT_VARS_H__
