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
	VAR_NO_ARGS("Total-block-count", VAR_TOTAL_BLOCK_COUNT),
	VAR_NO_ARGS("max-download-size", VAR_DOWNLOAD_SIZE),
	VAR_NO_ARGS("is-userspace", VAR_IS_USERSPACE),
	VAR_ARGS("partition-size", ':', VAR_PARTITION_SIZE),
	VAR_ARGS("partition-type", ':', VAR_PARTITION_TYPE),
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
	char var_buf[FASTBOOT_MSG_LEN_WO_PREFIX];

	for (int i = 0; fastboot_vars[i].name != NULL; i++) {
		fastboot_getvar_info_t *var = &fastboot_vars[i];
		const size_t val_max_len = FASTBOOT_MSG_LEN_WO_PREFIX - strlen(var->name) - 1;

		if (var->has_args) {
			fastboot_getvar_result_t state;
			int arg = 0;

			do {
				state = fastboot_getvar(fb, var->var, NULL, arg++,
							var_buf, val_max_len);
				if (state == STATE_OK)
					fastboot_info(fb, "%s:%s", var->name, var_buf);
				/*
				 * Disk error is the only error that may occur before we
				 * determine that we are on the last valid arg value. We can
				 * ignore other errors as we eventually hit STATE_LAST.
				 */
			} while (state != STATE_LAST && state != STATE_DISK_ERROR);
		} else {
			if (fastboot_getvar(fb, var->var, NULL, 0, var_buf, val_max_len) ==
			    STATE_OK)
				fastboot_info(fb, "%s:%s", var->name, var_buf);
		}
	}
	fastboot_succeed(fb);
}

void fastboot_cmd_getvar(struct FastbootOps *fb, char *args)
{
	char var_buf[FASTBOOT_MSG_LEN_WO_PREFIX];

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

		fastboot_getvar_result_t state = fastboot_getvar(
			fb, var->var, args, 0, var_buf, sizeof(var_buf));
		if (state == STATE_OK)
			fastboot_okay(fb, "%s", var_buf);
		else if (state == STATE_OVERFLOW)
			fastboot_fail(fb, "getvar truncated: %s", var_buf);
		else
			fastboot_fail(fb, "getvar failed - internal error");
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

/* Helper function to get partition type string based on name */
static const char *get_partition_type_string(const char *name)
{
	if (!strcmp(name, "OEM"))
		return "ext4";
	else if (!strcmp(name, "EFI-SYSTEM"))
		return "vfat";
	else if (!strcmp(name, "metadata"))
		return "ext4";
	else if (!strcmp(name, "userdata"))
		return "ext4";

	return "raw"; /* All other partitions are "raw" type*/
}

static int get_partition_var(fastboot_var_t var, GptData *gpt, GptEntry *e,
			     const char *part_name, char *outbuf, size_t len)
{
	switch (var) {
	case VAR_PARTITION_SIZE:
		return snprintf(outbuf, len, "0x%llx", GptGetEntrySizeBytes(gpt, e));
	case VAR_PARTITION_TYPE:
		return snprintf(outbuf, len, "%s", get_partition_type_string(part_name));
	default:
		die("Incorrect %s() argument: %u\n", __func__, var);
	}
}

fastboot_getvar_result_t fastboot_getvar(struct FastbootOps *fb, fastboot_var_t var,
					 char *arg, size_t index, char *outbuf,
					 size_t outbuf_len)
{
	fastboot_getvar_result_t state;
	GptEntry *part = NULL;
	int used_len = 0;
	int ret;
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

		used_len = snprintf(outbuf, outbuf_len, "%s", name);
		free(name);
		break;
	}
	case VAR_TOTAL_BLOCK_COUNT:
		if (fastboot_disk_init_no_fail(fb))
			return STATE_DISK_ERROR;
		used_len = snprintf(outbuf, outbuf_len, "0x%llx", fb->disk->block_count);
		break;
	case VAR_DOWNLOAD_SIZE:
		used_len = snprintf(outbuf, outbuf_len, "0x%llx", FASTBOOT_MAX_DOWNLOAD_SIZE);
		break;
	case VAR_IS_USERSPACE:
		used_len = snprintf(outbuf, outbuf_len, "no");
		break;
	case VAR_PARTITION_SIZE:
	case VAR_PARTITION_TYPE:
		if (fastboot_disk_gpt_init_no_fail(fb))
			return STATE_DISK_ERROR;
		if (arg != NULL) {
			name = arg;
			part = gpt_find_partition(fb->gpt, name);
			if (part == NULL)
				return STATE_UNKNOWN_VAR;
		} else {
			state = fastboot_get_partition_name_by_index(fb->gpt, index, &name,
								     &part);
			if (state != STATE_OK)
				return state;
			used_len = snprintf(outbuf, outbuf_len, "%s:", name);
			if (used_len < 0 || used_len > outbuf_len) {
				free(name);
				break;
			}
			outbuf += used_len;
		}
		ret = get_partition_var(var, fb->gpt, part, name, outbuf,
					outbuf_len - used_len);
		if (name != arg)
			free(name);
		if (ret < 0)
			used_len = ret;
		else
			used_len += ret;
		break;
	case VAR_PRODUCT: {
		struct cb_mainboard *mainboard =
			phys_to_virt(lib_sysinfo.cb_mainboard);
		const char *mb_part_string = cb_mb_part_string(mainboard);
		used_len = snprintf(outbuf, outbuf_len, "%s", mb_part_string);
		break;
	}
	case VAR_SLOT_COUNT:
		if (fastboot_disk_gpt_init_no_fail(fb))
			return STATE_DISK_ERROR;
		used_len = snprintf(outbuf, outbuf_len, "%d",
				    fastboot_get_slot_count(fb->gpt));
		break;
	case VAR_SECURE:
		used_len = snprintf(outbuf, outbuf_len, "no");
		break;
	case VAR_VERSION:
		used_len = snprintf(outbuf, outbuf_len, "0.4");
		break;
	case VAR_VERSION_BOOTLOADER: {
		const char *fw_id = get_active_fw_id();
		if (fw_id == NULL)
			used_len = snprintf(outbuf, outbuf_len, "unknown");
		else
			used_len = snprintf(outbuf, outbuf_len, "%s", fw_id);
		break;
	}
	case VAR_SLOT_SUFFIXES:
		if (fastboot_disk_gpt_init_no_fail(fb))
			return STATE_DISK_ERROR;
		used_len = fastboot_get_slot_suffixes(fb->gpt, outbuf, outbuf_len);
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
			used_len = snprintf(outbuf, outbuf_len, "%c:", slot);
			if (used_len < 0 || used_len > outbuf_len)
				break;
			outbuf += used_len;
		}
		ret = get_slot_var(var, part, outbuf, outbuf_len - used_len);
		if (ret < 0)
			used_len = ret;
		else
			used_len += ret;
		break;
	case VAR_LOGICAL_BLOCK_SIZE:
		if (fastboot_disk_init_no_fail(fb))
			return STATE_DISK_ERROR;

		used_len = snprintf(outbuf, outbuf_len, "0x%x", fb->disk->block_size);
		break;
	case VAR_SERIALNO: {
		u32 vpd_size;
		const unsigned char *vpd_data = vpd_find("serial_number", NULL, NULL,
							 &vpd_size);
		if (vpd_data && vpd_size > 0)
			used_len = snprintf(outbuf, outbuf_len, "%.*s",
					       (int)vpd_size, vpd_data);
		else
			used_len = snprintf(outbuf, outbuf_len, "unknown");
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
				char name_a[GPTENTRY_NAME_LEN + 1];
				ret = snprintf(name_a, sizeof(name_a), "%s_a", arg);
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
			used_len = snprintf(outbuf, outbuf_len, "%.*s:", name_len, name);
			free(name);
			if (used_len < 0 || used_len > outbuf_len)
				break;
			outbuf += used_len;
		}
		ret = snprintf(outbuf, outbuf_len - used_len, slot ? "yes" : "no");
		if (ret < 0)
			used_len = ret;
		else
			used_len += ret;
		break;
	default:
		return STATE_UNKNOWN_VAR;
	}

	if (used_len < 0) {
		printf("%s(): snprintf failed(%d) for var %d, arg \"%s\", index %ld\n",
		       __func__, used_len, var, arg, index);
		return STATE_PARSING_ERROR;
	}

	if (used_len > outbuf_len) {
		printf("%s(): too long output for var %d, arg \"%s\", index %ld\n",
		       __func__, var, arg, index);
		return STATE_OVERFLOW;
	}

	return STATE_OK;
}
