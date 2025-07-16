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
#include <ctype.h>
#include <string.h>
#include <sysinfo.h>

#include "base/gpt.h"
#include "fastboot/disk.h"
#include "fastboot/fastboot.h"
#include "fastboot/vars.h"
#include "base/vpd_util.h"
#include "vboot/firmware_id.h"

#define VAR_ARGS(_name, _sep, _var)                                            \
	{                                                                      \
		.name = _name, .has_args = true, .sep = _sep, .var = _var      \
	}
#define VAR_NO_ARGS(_name, _var)                                               \
	{                                                                      \
		.name = _name, .has_args = false, .var = _var                  \
	}
static fastboot_getvar_info_t fastboot_vars[] = {
	VAR_NO_ARGS("current-slot", VAR_CURRENT_SLOT),
	VAR_NO_ARGS("max-download-size", VAR_DOWNLOAD_SIZE),
	VAR_NO_ARGS("is-userspace", VAR_IS_USERSPACE),
	VAR_ARGS("partition-size", ':', VAR_PARTITION_SIZE),
	VAR_NO_ARGS("product", VAR_PRODUCT),
	VAR_NO_ARGS("secure", VAR_SECURE),
	VAR_NO_ARGS("slot-count", VAR_SLOT_COUNT),
	VAR_NO_ARGS("version", VAR_VERSION),
	VAR_NO_ARGS("version-bootloader", VAR_VERSION_BOOTLOADER),
	VAR_NO_ARGS("slot-suffixes", VAR_SLOT_SUFFIXES),
	VAR_ARGS("slot-successful", ':', VAR_SLOT_SUCCESSFUL),
	VAR_ARGS("slot-retry-count", ':', VAR_SLOT_RETRY_COUNT),
	VAR_ARGS("slot-unbootable", ':', VAR_SLOT_UNBOOTABLE),
	VAR_NO_ARGS("logical-block-size", VAR_LOGICAL_BLOCK_SIZE),
	VAR_NO_ARGS("serialno", VAR_SERIALNO),
	VAR_ARGS("has-slot", ':', VAR_HAS_SLOT),
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

				state = fastboot_getvar(fb, var->var, NULL, arg++,
							var_buf, &len);
				if (state == STATE_OK)
					fastboot_info(fb, "%s:%.*s", var->name, (int)len,
						      var_buf);
			} while (state == STATE_OK || state == STATE_TRY_NEXT);
		} else {
			size_t len = FASTBOOT_MSG_MAX;
			if (fastboot_getvar(fb, var->var, NULL, 0, var_buf, &len) == STATE_OK)
				fastboot_info(fb, "%s:%.*s", var->name, (int)len, var_buf);
		}
	}
	fastboot_succeed(fb);
}

void fastboot_cmd_getvar(struct FastbootOps *fb, char *args)
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
			fb, var->var, args, 0, var_buf, &var_len);
		if (state == STATE_OK) {
			fastboot_okay(fb, "%.*s", (int)var_len, var_buf);
		} else {
			fastboot_fail(fb, "getvar failed - internal error");
		}
		return;
	}

	fastboot_fail(fb, "Unknown variable");
}

static fastboot_getvar_result_t fastboot_get_partition_name_by_index(
		GptData *gpt, size_t index, char **name, GptEntry **part)
{
	if (gpt_get_number_of_partitions(gpt) <= index)
		return STATE_LAST;

	*part = gpt_get_partition(gpt, index);
	if (*part == NULL)
		return STATE_TRY_NEXT;

	*name = gpt_get_entry_name(*part);
	if (*name == NULL)
		return STATE_TRY_NEXT;

	return STATE_OK;
}

static fastboot_getvar_result_t fastboot_get_kernel_slot_by_index(
	GptData *gpt, size_t index, GptEntry **out_entry, char *out_slot_char)
{
	char *name;
	fastboot_getvar_result_t state;

	state = fastboot_get_partition_name_by_index(gpt, index, &name, out_entry);
	if (state != STATE_OK)
		return state;

	if (!IsAndroid(*out_entry))
		return STATE_TRY_NEXT;

	*out_slot_char = fastboot_get_slot_for_partition_name(name);
	free(name);
	if (*out_slot_char == 0)
		return STATE_TRY_NEXT;

	return STATE_OK;
}

static bool is_entry_unbootable(GptEntry *entry)
{
	return !IsBootableEntry(entry) || GetEntryPriority(entry) == 0 ||
		(!GetEntrySuccessful(entry) &&  GetEntryTries(entry) == 0);
}

static int get_slot_var(fastboot_var_t var, GptEntry *e, char *outbuf, size_t len)
{
	switch (var) {
	case VAR_SLOT_SUCCESSFUL:
		return snprintf(outbuf, len, "%s", GetEntrySuccessful(e) ? "yes" : "no");
	case VAR_SLOT_RETRY_COUNT:
		return snprintf(outbuf, len, "%d", GetEntryTries(e));
	case VAR_SLOT_UNBOOTABLE:
		return snprintf(outbuf, len, "%s", is_entry_unbootable(e) ? "yes" : "no");
	default:
		die("Incorrect %s() argument: %u\n", __func__, var);
	}
}

fastboot_getvar_result_t fastboot_getvar(struct FastbootOps *fb, fastboot_var_t var,
					 const char *arg, size_t index, char *outbuf,
					 size_t *outbuf_len)
{
	fastboot_getvar_result_t state;
	GptEntry *part = NULL;
	size_t used_len = 0;
	char *name;
	char slot = 0;

	switch (var) {
	case VAR_CURRENT_SLOT: {
		if (fastboot_disk_gpt_init_no_fail(fb))
			return STATE_DISK_ERROR;

		/* Make sure that GptNextKernelEntry starts with fresh state */
		if (GptInit(fb->gpt) != GPT_SUCCESS)
			return STATE_DISK_ERROR;

		part = GptNextKernelEntry(fb->gpt);
		if (part == NULL)
			return STATE_DISK_ERROR;

		name = gpt_get_entry_name(part);
		if (name == NULL || name[0] == '\0')
			return STATE_DISK_ERROR;
		/* Get last character */
		while (name[1] != '\0')
			name++;

		used_len = snprintf(outbuf, *outbuf_len, "%s", name);
		free(name);
		break;
	}
	case VAR_DOWNLOAD_SIZE:
		used_len = snprintf(outbuf, *outbuf_len, "0x%llx", FASTBOOT_MAX_DOWNLOAD_SIZE);
		break;
	case VAR_IS_USERSPACE:
		used_len = snprintf(outbuf, *outbuf_len, "no");
		break;
	case VAR_PARTITION_SIZE:
		if (fastboot_disk_gpt_init_no_fail(fb))
			return STATE_DISK_ERROR;
		if (arg != NULL) {
			part = gpt_find_partition(fb->gpt, arg);
			if (part == NULL)
				return STATE_UNKNOWN_VAR;
		} else {
			state = fastboot_get_partition_name_by_index(fb->gpt, index, &name,
								     &part);
			if (state != STATE_OK)
				return state;
			used_len = snprintf(outbuf, *outbuf_len, "%s:", name);
			outbuf += used_len;
			*outbuf_len -= used_len;
			free(name);
		}

		used_len += snprintf(outbuf, *outbuf_len, "0x%llx",
				     GptGetEntrySizeBytes(fb->gpt, part));
		break;
	case VAR_PRODUCT: {
		struct cb_mainboard *mainboard =
			phys_to_virt(lib_sysinfo.cb_mainboard);
		const char *mb_part_string = cb_mb_part_string(mainboard);
		used_len = snprintf(outbuf, *outbuf_len, "%s", mb_part_string);
		break;
	}
	case VAR_SLOT_COUNT:
		if (fastboot_disk_gpt_init_no_fail(fb))
			return STATE_DISK_ERROR;
		used_len = snprintf(outbuf, *outbuf_len, "%d",
				    fastboot_get_slot_count(fb->gpt));
		break;
	case VAR_SECURE:
		used_len = snprintf(outbuf, *outbuf_len, "no");
		break;
	case VAR_VERSION:
		used_len = snprintf(outbuf, *outbuf_len, "0.4");
		break;
	case VAR_VERSION_BOOTLOADER: {
		const char *fw_id = get_active_fw_id();
		if (fw_id == NULL)
			used_len = snprintf(outbuf, *outbuf_len, "unknown");
		else
			used_len = snprintf(outbuf, *outbuf_len, "%s", fw_id);
		break;
	}
	case VAR_SLOT_SUFFIXES:
		if (fastboot_disk_gpt_init_no_fail(fb))
			return STATE_DISK_ERROR;
		used_len = fastboot_get_slot_suffixes(fb->gpt, outbuf, *outbuf_len);
		break;
	case VAR_SLOT_SUCCESSFUL:
	case VAR_SLOT_RETRY_COUNT:
	case VAR_SLOT_UNBOOTABLE:
		if (fastboot_disk_gpt_init_no_fail(fb))
			return STATE_DISK_ERROR;

		if (arg != NULL) {
			if (strlen(arg) != 1)
				return STATE_UNKNOWN_VAR;
			slot = arg[0];
			part = fastboot_get_kernel_for_slot(fb->gpt, slot);
			if (part == NULL)
				return STATE_UNKNOWN_VAR;
		} else {
			state = fastboot_get_kernel_slot_by_index(fb->gpt, index, &part, &slot);
			if (state != STATE_OK)
				return state;
			used_len = snprintf(outbuf, *outbuf_len, "%c:", slot);
			outbuf += used_len;
			*outbuf_len -= used_len;
		}
		used_len += get_slot_var(var, part, outbuf, *outbuf_len);
		break;
	case VAR_LOGICAL_BLOCK_SIZE:
		if (fastboot_disk_init_no_fail(fb))
			return STATE_DISK_ERROR;

		used_len = snprintf(outbuf, *outbuf_len, "0x%x", fb->disk->block_size);
		break;
	case VAR_SERIALNO: {
		u32 vpd_size;
		const unsigned char *vpd_data = vpd_find("serial_number", NULL, NULL,
							 &vpd_size);
		if (vpd_data && vpd_size > 0)
			used_len = snprintf(outbuf, *outbuf_len, "%.*s",
					       (int)vpd_size, vpd_data);
		else
			used_len = snprintf(outbuf, *outbuf_len, "unknown");
		break;
	}
	case VAR_HAS_SLOT:
		if (fastboot_disk_gpt_init_no_fail(fb))
			return STATE_DISK_ERROR;
		if (arg != NULL) {
			slot = fastboot_get_slot_for_partition_name(arg);
			if (slot)
				return STATE_UNKNOWN_VAR;
			part = gpt_find_partition(fb->gpt, arg);
			if (part) {
				slot = 0;
			} else {
				/* Maximum length of GPT name is 36 chars */
				char name_a[37];
				int ret = snprintf(name_a, sizeof(name_a), "%s_a", arg);
				if (ret < 0 || ret > sizeof(name_a))
					return STATE_UNKNOWN_VAR;
				part = gpt_find_partition(fb->gpt, name_a);
				if (!part)
					return STATE_UNKNOWN_VAR;
				slot = 'a';
			}
		} else {
			int name_len;
			state = fastboot_get_partition_name_by_index(fb->gpt, index, &name,
								     &part);
			if (state != STATE_OK)
				return state;
			name_len = strlen(name);
			slot = fastboot_get_slot_for_partition_name(name);
			if (slot) {
				if (slot != 'a')
					return STATE_TRY_NEXT;
				name_len -= 2;
			}
			used_len = snprintf(outbuf, *outbuf_len, "%.*s:", name_len, name);
			outbuf += used_len;
			*outbuf_len -= used_len;
			free(name);
		}
		used_len += snprintf(outbuf, *outbuf_len, slot ? "yes" : "no");
		break;
	default:
		return STATE_UNKNOWN_VAR;
	}

	*outbuf_len = used_len;
	return STATE_OK;
}
