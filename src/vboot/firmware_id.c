/*
 * Copyright 2015 Google Inc.
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

#include <libpayload.h>
#include <vboot_struct.h>

#include "image/fmap.h"
#include "vboot/firmware_id.h"
#include "vboot/util/commonparams.h"

static struct fwid {
	int vbsd_id;
	const char *fw_name;
	const char *fw_id;
	int fw_size;
	const char *id_err;
} fw_fmap_ops[] = {
	{VBSD_RW_A, "RW_FWID_A", NULL, 0, "RW A: ID NOT FOUND"},
	{VBSD_RW_B, "RW_FWID_B", NULL, 0, "RW B: ID NOT FOUND"},
	{VBSD_RECOVERY, "RO_FRID", NULL, 0, "RO: ID NOT FOUND"},
};

static void get_fw_details(struct fwid *entry)
{
	if (entry->fw_id)
		return;

	entry->fw_id = fmap_find_string(entry->fw_name, &entry->fw_size);

	if (entry->fw_id == NULL)
		entry->fw_id = entry->id_err;
}

static struct fwid *get_fw_entry(int index)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(fw_fmap_ops); i++) {
		if (fw_fmap_ops[i].vbsd_id == index)
			break;
	}

	if (i == ARRAY_SIZE(fw_fmap_ops))
		return NULL;

	struct fwid *entry = &fw_fmap_ops[i];

	get_fw_details(entry);

	return entry;
}

const char *get_fw_id(int index)
{
	struct fwid *entry = get_fw_entry(index);

	if(entry == NULL)
		return "NOT FOUND";

	return entry->fw_id;
}

int get_fw_size(int index)
{
	struct fwid *entry = get_fw_entry(index);

	if (entry == NULL)
		return 0;

	return entry->fw_size;
}

const char *get_ro_fw_id(void)
{
	return get_fw_id(VBSD_RECOVERY);
}

const char *get_rwa_fw_id(void)
{
	return get_fw_id(VBSD_RW_A);
}

const char *get_rwb_fw_id(void)
{
	return get_fw_id(VBSD_RW_B);
}

int get_ro_fw_size(void)
{
	return get_fw_size(VBSD_RECOVERY);
}

int get_rwa_fw_size(void)
{
	return get_fw_size(VBSD_RW_A);
}

int get_rwb_fw_size(void)
{
	return get_fw_size(VBSD_RW_B);
}

static VbSharedDataHeader *get_vb_sd(void)
{
	void *blob;
	int size;

	if (find_common_params(&blob, &size) == 0)
		return blob;

	return NULL;
}

static inline int get_active_fw_index(VbSharedDataHeader *vb_sd)
{
	int fw_index = VBSD_UNKNOWN;

	if (vb_sd)
		fw_index = vb_sd->firmware_index;

	return fw_index;
}

const char *get_active_fw_id(void)
{
	VbSharedDataHeader *vb_sd = get_vb_sd();
	int fw_index = get_active_fw_index(vb_sd);

	return get_fw_id(fw_index);
}

int get_active_fw_size(void)
{
	VbSharedDataHeader *vb_sd = get_vb_sd();
	int fw_index = get_active_fw_index(vb_sd);

	return get_fw_size(fw_index);
}
