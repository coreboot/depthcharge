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

#include <arch/virtual.h>
#include <string.h>
#include <sysinfo.h>

#include "fastboot/disk.h"
#include "fastboot/fastboot.h"
#include "fastboot/vars.h"

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
	VAR_NO_ARGS("is-userspace", VAR_IS_USERSPACE),
	VAR_ARGS("partition-size", ':', VAR_PARTITION_SIZE),
	VAR_NO_ARGS("product", VAR_PRODUCT),
	VAR_NO_ARGS("secure", VAR_SECURE),
	VAR_NO_ARGS("slot-count", VAR_SLOT_COUNT),
	VAR_NO_ARGS("version", VAR_VERSION),
	{.name = NULL},
};

static void fastboot_getvar_all(struct FastbootOps *fb)
{
	char var_buf[FASTBOOT_MSG_MAX];

	for (int i = 0; fastboot_vars[i].name != NULL; i++) {
		fastboot_getvar_info_t *var = &fastboot_vars[i];

		if (var->has_args) {
			fastboot_getvar_result_t state;
			int arg = 0;

			do {
				size_t len = FASTBOOT_MSG_MAX;

				state = fastboot_getvar(var->var, NULL, arg++, var_buf, &len);
				if (state == STATE_OK)
					fastboot_info(fb, "%s:%.*s", var->name, (int)len,
						      var_buf);
			} while (state == STATE_OK || state == STATE_TRY_NEXT);
		} else {
			size_t len = FASTBOOT_MSG_MAX;
			fastboot_getvar(var->var, NULL, 0, var_buf, &len);
			fastboot_info(fb, "%s:%.*s", var->name, (int)len,
				      var_buf);
		}
	}
	fastboot_succeed(fb);
}

void fastboot_cmd_getvar(struct FastbootOps *fb, const char *args)
{
	char var_buf[FASTBOOT_MSG_MAX];

	if (!strcmp(args, "all")) {
		fastboot_getvar_all(fb);
		return;
	}
	for (int i = 0; fastboot_vars[i].name != NULL; i++) {
		fastboot_getvar_info_t *var = &fastboot_vars[i];
		int name_len = strlen(var->name);

		if (strncmp(var->name, args, name_len))
			continue;

		if (var->has_args) {
			if (args[name_len] != var->sep)
				continue;
			args++;
		} else if (args[name_len] != '\0')
			continue;
		args += name_len;

		size_t var_len = FASTBOOT_MSG_MAX;
		fastboot_getvar_result_t state = fastboot_getvar(
			var->var, args, 0, var_buf, &var_len);
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
					 size_t index, char *outbuf,
					 size_t *outbuf_len)
{
	size_t used_len = 0;
	switch (var) {
	case VAR_DOWNLOAD_SIZE:
		used_len = snprintf(outbuf, *outbuf_len, "%llu",
				    FASTBOOT_MAX_DOWNLOAD_SIZE);
		break;
	case VAR_IS_USERSPACE:
		used_len = snprintf(outbuf, *outbuf_len, "no");
		break;
	case VAR_PARTITION_SIZE: {
		struct fastboot_disk disk;
		GptEntry *part = NULL;

		if (!fastboot_disk_init(&disk))
			return STATE_DISK_ERROR;
		if (arg != NULL) {
			part = fastboot_find_partition(&disk, arg);
			if (part == NULL)
				return STATE_UNKNOWN_VAR;
		} else {
			char *name;

			/* There is no more partitions to get */
			if (fastboot_get_number_of_partitions(&disk) <= index)
				return STATE_LAST;

			part = fastboot_get_partition(&disk, index);
			if (part == NULL)
				return STATE_TRY_NEXT;

			name = fastboot_get_entry_name(part);
			if (name == NULL)
				return STATE_TRY_NEXT;

			used_len = snprintf(outbuf, *outbuf_len, "%s:", name);
			outbuf += used_len;
			*outbuf_len -= used_len;
			free(name);
		}

		used_len += snprintf(outbuf, *outbuf_len, "0x%llx",
				     GptGetEntrySizeBytes(disk.gpt, part));
		fastboot_disk_destroy(&disk);
		break;
	}
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
