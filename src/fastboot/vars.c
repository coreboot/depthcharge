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

#include "fastboot/vars.h"

#include <arch/virtual.h>
#include <sysinfo.h>
#include "fastboot/disk.h"
#include "fastboot/fastboot.h"

#include <string.h>

#define VAR_ARGS(_name, _sep, _var)                                            \
	{                                                                      \
		.name = _name, .has_args = true, .sep = _sep, .var = _var      \
	}
#define VAR_NO_ARGS(_name, _var)                                               \
	{                                                                      \
		.name = _name, .has_args = false, .var = _var                  \
	}
static fastboot_getvar_info_t fastboot_vars[] = {
	VAR_NO_ARGS("max-download-size", VAR_DOWNLOAD_SIZE),
	VAR_NO_ARGS("product", VAR_PRODUCT),
	VAR_NO_ARGS("secure", VAR_SECURE),
	VAR_NO_ARGS("slot-count", VAR_SLOT_COUNT),
	VAR_NO_ARGS("version", VAR_VERSION),
	{.name = NULL},
};

static char var_buf[FASTBOOT_MSG_MAX];
static void fastboot_getvar_all(fastboot_session_t *fb)
{
	for (int i = 0; fastboot_vars[i].name != NULL; i++) {
		fastboot_getvar_info_t *var = &fastboot_vars[i];

		if (var->has_args) {
			// TODO(fxbug.dev/80267): enumerate arguments.
			fastboot_info(fb, "%s:not yet supported", var->name);
			continue;
		} else {
			size_t len = FASTBOOT_MSG_MAX;
			fastboot_getvar(var->var, NULL, 0, var_buf, &len);
			fastboot_info(fb, "%s:%.*s", var->name, (int)len,
				      var_buf);
		}
	}
	fastboot_succeed(fb);
}

void fastboot_cmd_getvar(fastboot_session_t *fb, const char *args,
			 uint64_t arg_len)
{
	if (arg_len == strlen("all") && !strncmp(args, "all", strlen("all"))) {
		fastboot_getvar_all(fb);
		return;
	}
	for (int i = 0; fastboot_vars[i].name != NULL; i++) {
		fastboot_getvar_info_t *var = &fastboot_vars[i];
		int name_len = strlen(var->name);
		if (name_len > arg_len)
			continue;

		if (strncmp(var->name, args, name_len))
			continue;

		args += name_len;
		arg_len -= name_len;
		if (var->has_args) {
			if (arg_len < 1 || args[0] != var->sep) {
				args -= name_len;
				arg_len += name_len;
				continue;
			}
			args++;
			arg_len--;
		}

		size_t var_len = FASTBOOT_MSG_MAX;
		fastboot_getvar_result_t state = fastboot_getvar(
			var->var, args, arg_len, var_buf, &var_len);
		if (state == STATE_OK) {
			fastboot_okay(fb, "%.*s", (int)var_len, var_buf);
		} else {
			fastboot_fail(fb, "getvar failed - internal error");
		}
		return;
	}

	fastboot_fail(fb, "Unknown variable");
}

fastboot_getvar_result_t fastboot_getvar(fastboot_var_t var, const char *arg,
					 size_t arg_len, char *outbuf,
					 size_t *outbuf_len)
{
	size_t used_len = 0;
	switch (var) {
	case VAR_DOWNLOAD_SIZE:
		used_len = snprintf(outbuf, *outbuf_len, "%llu",
				    FASTBOOT_MAX_DOWNLOAD_SIZE);
		break;
	case VAR_PRODUCT: {
		struct cb_mainboard *mainboard =
			phys_to_virt(lib_sysinfo.cb_mainboard);
		const char *mb_part_string = cb_mb_part_string(mainboard);
		used_len = snprintf(outbuf, *outbuf_len, "%s", mb_part_string);
		break;
	}
	case VAR_SLOT_COUNT: {
		struct fastboot_disk disk;
		if (!fastboot_disk_init(&disk))
			return STATE_DISK_ERROR;
		used_len = snprintf(outbuf, *outbuf_len, "%d",
				    fastboot_get_slot_count(&disk));
		fastboot_disk_destroy(&disk);
		break;
	}
	case VAR_SECURE:
		used_len = snprintf(outbuf, *outbuf_len, "no");
		break;
	case VAR_VERSION:
		used_len = snprintf(outbuf, *outbuf_len, "0.4");
		break;
	default:
		return STATE_UNKNOWN_VAR;
	}

	*outbuf_len = used_len;
	return STATE_OK;
}
